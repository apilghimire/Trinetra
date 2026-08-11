"""
Online vision backend: Gemini Flash over the official Live API (WebSocket
under the hood, via Google's `google-genai` SDK).

Keeps one persistent, pre-authenticated Live session open so a query only
costs one send + one receive (no per-query TLS/HTTP handshake) -- that's the
"instant result" this buys over a plain REST call. Runs its own asyncio event
loop in a background thread so the rest of this codebase (subprocess- and
thread-based, see assistant.py/stt.py/tts.py) can call it synchronously.

Uses the `google-genai` SDK's `client.aio.live.connect()` rather than a
hand-rolled WebSocket client: it's Google's officially maintained interface
to the Live API, so model routing and the wire protocol (message shapes,
API version) are handled internally instead of being reimplemented here --
an earlier hand-rolled version of this file kept getting rejected with
"model not found for API version v1alpha" for every model name tried, which
turned out to be a symptom of a wrong-shaped API key rather than a genuinely
wrong model, but that failure mode is exactly what the official SDK exists
to avoid.

Requires a `GEMINI_API_KEY` (env var, or passed explicitly) and a working
internet connection -- see `hybrid_assistant.py` for the online/offline
switch that falls back to the local SmolVLM2 model when either is missing.

`GEMINI_API_KEY` must be a standard Gemini Developer API key from
https://aistudio.google.com/apikey (format `AIzaSy...`, 39 chars) -- NOT an
ephemeral token from the AI Studio "Stream Realtime" playground (format
`AQ....`), which is scoped to a single playground session/model and gets
rejected here.
"""

from __future__ import annotations

import asyncio
import os
import sys
import threading
from typing import Optional

from google import genai
from google.genai import types

DEFAULT_MODEL = "gemini-3.1-flash-live-preview"
DEFAULT_CONNECT_TIMEOUT = 10.0
DEFAULT_RESPONSE_TIMEOUT = 8.0


class GeminiLiveClient:
    """Persistent Live API session to Gemini, via the google-genai SDK."""

    def __init__(
        self,
        api_key: Optional[str] = None,
        model: str = DEFAULT_MODEL,
        connect_timeout: float = DEFAULT_CONNECT_TIMEOUT,
    ) -> None:
        self.api_key = api_key or os.environ.get("GEMINI_API_KEY")
        self.model = model
        self.connect_timeout = connect_timeout
        self._loop: Optional[asyncio.AbstractEventLoop] = None
        self._thread: Optional[threading.Thread] = None
        self._client: Optional[genai.Client] = None
        self._session_cm = None  # the async context manager from client.aio.live.connect()
        self._session = None  # the entered AsyncSession
        self._connected = False

    @property
    def is_connected(self) -> bool:
        return self._connected

    def start(self) -> "GeminiLiveClient":
        """Start the background event loop (does not connect yet)."""
        self._loop = asyncio.new_event_loop()

        def _run(loop: asyncio.AbstractEventLoop) -> None:
            asyncio.set_event_loop(loop)
            try:
                loop.run_forever()
            finally:
                loop.close()

        self._thread = threading.Thread(target=_run, args=(self._loop,), daemon=True, name="gemini-live-loop")
        self._thread.start()
        return self

    def connect(self, timeout: Optional[float] = None) -> bool:
        """Synchronously (re)connect the persistent Live session. Returns True on success."""
        if not self.api_key:
            print("[online] no GEMINI_API_KEY set; skipping online connect", file=sys.stderr)
            self._connected = False
            return False
        assert self._loop is not None, "call start() first"
        future = asyncio.run_coroutine_threadsafe(self._connect_async(), self._loop)
        try:
            future.result(timeout=timeout or self.connect_timeout)
            return True
        except Exception as exc:
            print(f"[online] Gemini Live connect failed: {exc}", file=sys.stderr)
            self._connected = False
            return False

    async def _connect_async(self) -> None:
        if self._session is not None:
            try:
                await self._session_cm.__aexit__(None, None, None)
            except Exception:
                pass
            self._session = None
            self._session_cm = None
        if self._client is None:
            self._client = genai.Client(api_key=self.api_key)
        # Current Live-API Flash models are voice-first and reject a TEXT-only
        # response_modalities request ("combination of response modalities
        # (TEXT) is not supported"). Ask for AUDIO (required) but enable
        # output transcription and read the transcript instead of the audio --
        # we only need text here, not spoken output.
        config = types.LiveConnectConfig(
            response_modalities=["AUDIO"],
            output_audio_transcription=types.AudioTranscriptionConfig(),
        )
        self._session_cm = self._client.aio.live.connect(model=self.model, config=config)
        self._session = await self._session_cm.__aenter__()
        self._connected = True

    def ask(self, image_bytes: bytes, question: str, mime_type: str = "image/jpeg", timeout: Optional[float] = None) -> str:
        """Send one image + question turn, block for the full text answer."""
        if not self._connected or self._session is None:
            raise RuntimeError("Gemini Live client is not connected")
        assert self._loop is not None
        future = asyncio.run_coroutine_threadsafe(
            self._ask_async(image_bytes, question, mime_type, timeout or DEFAULT_RESPONSE_TIMEOUT), self._loop
        )
        try:
            return future.result(timeout=(timeout or DEFAULT_RESPONSE_TIMEOUT) + 2.0)
        except Exception:
            # Any failure (dropped session, timeout, malformed reply) invalidates
            # the persistent session -- let the caller fall back to offline and
            # let the next connect() (driven by NetworkMonitor) re-establish it.
            self._connected = False
            raise

    async def _ask_async(self, image_bytes: bytes, question: str, mime_type: str, timeout: float) -> str:
        content = types.Content(
            role="user",
            parts=[
                types.Part(text=question),
                types.Part.from_bytes(data=image_bytes, mime_type=mime_type),
            ],
        )
        await self._session.send_client_content(turns=content, turn_complete=True)

        chunks: list[str] = []

        async def _collect() -> None:
            async for message in self._session.receive():
                sc = message.server_content
                if sc and sc.output_transcription and sc.output_transcription.text:
                    chunks.append(sc.output_transcription.text)
                if sc and sc.turn_complete:
                    break

        await asyncio.wait_for(_collect(), timeout=timeout)
        return "".join(chunks).strip()

    def stop(self) -> None:
        if self._loop is not None and self._session is not None:
            fut = asyncio.run_coroutine_threadsafe(self._session_cm.__aexit__(None, None, None), self._loop)
            try:
                fut.result(timeout=5.0)
            except Exception:
                pass
        if self._loop is not None:
            self._loop.call_soon_threadsafe(self._loop.stop)
        if self._thread is not None:
            self._thread.join(timeout=5.0)
        self._connected = False


if __name__ == "__main__":
    import argparse
    from pathlib import Path

    parser = argparse.ArgumentParser(description="One-shot test of the Gemini Live online backend")
    parser.add_argument("image", help="Path to an image file")
    parser.add_argument("question", help="Question to ask about the image")
    parser.add_argument("--api-key", default=None, help="Overrides GEMINI_API_KEY env var")
    parser.add_argument("--model", default=DEFAULT_MODEL)
    args = parser.parse_args()

    client = GeminiLiveClient(api_key=args.api_key, model=args.model).start()
    if not client.connect():
        print("Failed to connect.", file=sys.stderr)
        sys.exit(1)
    try:
        answer = client.ask(Path(args.image).read_bytes(), args.question)
        print(answer)
    finally:
        client.stop()
