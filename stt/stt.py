"""
Offline speech-to-text module for the RDK X5 (8GB) using Vosk.

Streams audio from the onboard mic (ES8326 codec) and emits recognized
text in real time, fully offline (no network calls at inference time).
"""

from __future__ import annotations

import argparse
import json
import queue
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterator, Optional

import numpy as np
import sounddevice as sd
import vosk

DEFAULT_MODEL_DIR = Path(__file__).resolve().parent.parent / "models" / "vosk-model-small-en-us"
SAMPLE_RATE = 16000
BLOCK_SIZE = 8000  # ~0.5s of audio per callback at 16kHz
# The RDK X5 onboard codec (ES8326) only exposes a fixed 2-channel capture
# stream, even though speech recognition needs mono. Capture stereo and
# downmix in software instead of asking the device for mono.
CAPTURE_CHANNELS = 2


@dataclass
class Transcript:
    text: str
    is_final: bool


class OfflineSTT:
    """Streaming offline speech recognizer backed by Vosk."""

    def __init__(
        self,
        model_dir: str | Path = DEFAULT_MODEL_DIR,
        sample_rate: int = SAMPLE_RATE,
        device: Optional[int | str] = None,
    ) -> None:
        model_path = Path(model_dir)
        if not model_path.exists():
            raise FileNotFoundError(
                f"Vosk model not found at {model_path}. "
                "Download one from https://alphacephei.com/vosk/models "
                "and point model_dir at the extracted folder."
            )
        vosk.SetLogLevel(-1)
        self.model = vosk.Model(str(model_path))
        self.sample_rate = sample_rate
        self.device = device
        self._audio_q: "queue.Queue[bytes]" = queue.Queue()
        self._recognizer = vosk.KaldiRecognizer(self.model, self.sample_rate)

    def _callback(self, indata, frames, time_info, status) -> None:
        if status:
            print(f"[stt] audio status: {status}", file=sys.stderr)
        stereo = np.frombuffer(indata, dtype=np.int16).reshape(-1, CAPTURE_CHANNELS)
        mono = stereo.mean(axis=1).astype(np.int16)
        self._audio_q.put(mono.tobytes())

    def listen(self) -> Iterator[Transcript]:
        """Yield Transcript objects (partial and final) as speech is recognized."""
        with sd.RawInputStream(
            samplerate=self.sample_rate,
            blocksize=BLOCK_SIZE,
            device=self.device,
            dtype="int16",
            channels=CAPTURE_CHANNELS,
            callback=self._callback,
        ):
            while True:
                data = self._audio_q.get()
                if self._recognizer.AcceptWaveform(data):
                    result = json.loads(self._recognizer.Result())
                    text = result.get("text", "").strip()
                    if text:
                        yield Transcript(text=text, is_final=True)
                else:
                    partial = json.loads(self._recognizer.PartialResult())
                    text = partial.get("partial", "").strip()
                    if text:
                        yield Transcript(text=text, is_final=False)

    def run(self, on_transcript: Callable[[Transcript], None]) -> None:
        """Blocking helper: call on_transcript for every emitted Transcript."""
        for transcript in self.listen():
            on_transcript(transcript)

    def transcribe_file(self, wav_path: str | Path) -> str:
        """One-shot transcription of a 16kHz mono PCM WAV file."""
        import wave

        wav_path = Path(wav_path)
        with wave.open(str(wav_path), "rb") as wf:
            if wf.getnchannels() != 1 or wf.getsampwidth() != 2:
                raise ValueError("WAV file must be mono 16-bit PCM")
            recognizer = vosk.KaldiRecognizer(self.model, wf.getframerate())
            recognizer.SetWords(True)
            pieces = []
            while True:
                data = wf.readframes(4000)
                if not data:
                    break
                if recognizer.AcceptWaveform(data):
                    pieces.append(json.loads(recognizer.Result()).get("text", ""))
            pieces.append(json.loads(recognizer.FinalResult()).get("text", ""))
            return " ".join(p for p in pieces if p).strip()


def list_devices() -> None:
    print(sd.query_devices())


def main() -> None:
    parser = argparse.ArgumentParser(description="Offline STT (Vosk) for RDK X5")
    parser.add_argument("--model", default=str(DEFAULT_MODEL_DIR), help="Path to Vosk model dir")
    parser.add_argument("--device", default=None, help="Input audio device index or name")
    parser.add_argument("--file", default=None, help="Transcribe a WAV file instead of the mic")
    parser.add_argument("--list-devices", action="store_true", help="List audio devices and exit")
    parser.add_argument(
        "--partial", action="store_true", help="Also print partial (in-progress) results"
    )
    args = parser.parse_args()

    if args.list_devices:
        list_devices()
        return

    device = args.device
    if device is not None and device.isdigit():
        device = int(device)

    stt = OfflineSTT(model_dir=args.model, device=device)

    if args.file:
        print(stt.transcribe_file(args.file))
        return

    print("Listening... (Ctrl+C to stop)", file=sys.stderr)
    try:
        for transcript in stt.listen():
            if transcript.is_final:
                print(transcript.text)
            elif args.partial:
                print(f"... {transcript.text}", end="\r", file=sys.stderr)
    except KeyboardInterrupt:
        print("\nStopped.", file=sys.stderr)


if __name__ == "__main__":
    main()
