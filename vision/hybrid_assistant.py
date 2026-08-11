"""
Hybrid visual assistant for the RDK X5: online Gemini Flash when there's a
network, offline SmolVLM2 (BPU) always loaded as the backup.

Both backends are loaded up front -- the offline model (see assistant.py)
because it's the guaranteed fallback, and the Gemini Live WebSocket client
(see online_client.py) because a persistent connection is what makes the
online path near-instant instead of paying a fresh handshake per query.
A NetworkMonitor (see network_monitor.py) polls connectivity in the
background and reconnects the online client the moment a network appears;
`ask()` uses the online path whenever it's connected and transparently
falls back to the offline model on any online failure (timeout, dropped
connection, missing API key, network loss mid-query).

Unlike assistant.py's VisualAssistant, this is NOT offline-only -- when
online, an image and question are sent to Google's servers. Use
assistant.py directly if you need the fully-offline guarantee.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Optional

VISION_DIR = Path(__file__).resolve().parent
PROJECT_DIR = VISION_DIR.parent
sys.path.insert(0, str(VISION_DIR))
sys.path.insert(0, str(PROJECT_DIR / "stt"))
sys.path.insert(0, str(PROJECT_DIR / "tts"))

from assistant import VisualAssistant  # noqa: E402
from network_monitor import NetworkMonitor  # noqa: E402
from online_client import DEFAULT_MODEL as DEFAULT_GEMINI_MODEL  # noqa: E402
from online_client import GeminiLiveClient  # noqa: E402

DEFAULT_ONLINE_TIMEOUT = 8.0


class HybridVisualAssistant:
    """Routes each query to online Gemini Flash if available, else the offline model."""

    def __init__(
        self,
        api_key: Optional[str] = None,
        gemini_model: str = DEFAULT_GEMINI_MODEL,
        online_timeout: float = DEFAULT_ONLINE_TIMEOUT,
        **offline_kwargs,
    ) -> None:
        # Offline backup: loaded eagerly and unconditionally, same as a bare
        # VisualAssistant. This must succeed even with zero network/API key.
        self.offline = VisualAssistant(**offline_kwargs)

        self.online_timeout = online_timeout
        self.online = GeminiLiveClient(api_key=api_key, model=gemini_model).start()
        self.network = NetworkMonitor(on_change=self._on_network_change).start()
        if self.network.is_online:
            self.online.connect()

    def _on_network_change(self, online: bool) -> None:
        if online:
            print("[hybrid] network up -- connecting to Gemini Live", file=sys.stderr)
            self.online.connect()
        else:
            print("[hybrid] network down -- using offline model", file=sys.stderr)

    def ask(self, image_path: str | Path, question: str) -> str:
        image_path = Path(image_path)
        if not image_path.exists():
            raise FileNotFoundError(f"Image not found: {image_path}")

        if self.network.is_online and self.online.is_connected:
            try:
                answer = self.online.ask(image_path.read_bytes(), question, timeout=self.online_timeout)
                print("[hybrid] answered online (Gemini Flash)", file=sys.stderr)
                return answer
            except Exception as exc:
                print(f"[hybrid] online query failed ({exc}); falling back to offline", file=sys.stderr)

        print("[hybrid] answered offline (SmolVLM2 + BPU)", file=sys.stderr)
        return self.offline.ask(image_path, question)

    def ask_camera(
        self, question: str, save_capture: str | Path = "/tmp/rdk_capture.jpg", camera_index: int = 0
    ) -> str:
        """MIPI camera path -- UNVERIFIED on real hardware, see vision/README.md."""
        from camera import capture_photo  # local import: only needed on-device with hobot_vio

        image_path = capture_photo(save_capture, camera_index=camera_index)
        return self.ask(image_path, question)

    def ask_usb_camera(self, question: str, save_capture: str | Path = "/tmp/usb_capture.jpg") -> str:
        """USB webcam path -- verified working."""
        from camera import capture_usb_photo

        image_path = capture_usb_photo(save_capture)
        return self.ask(image_path, question)

    def listen_question(self, timeout: float = 15.0) -> Optional[str]:
        return self.offline.listen_question(timeout=timeout)

    def speak(self, text: str) -> None:
        self.offline.speak(text)

    def close(self) -> None:
        self.network.stop()
        self.online.stop()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Hybrid visual assistant (online Gemini Flash + offline SmolVLM2 backup) for RDK X5. "
        "Run with no arguments for the full voice system: listen for a question, capture a photo, "
        "answer (online if available, else offline), and speak the answer."
    )
    image_group = parser.add_mutually_exclusive_group()
    image_group.add_argument("--image", help="Path to an image file to ask about")
    image_group.add_argument(
        "--camera", action="store_true",
        help="Capture a fresh photo from the GW130W/GS130W MIPI camera (UNVERIFIED on real hardware)",
    )
    image_group.add_argument(
        "--usb-camera", action="store_true",
        help="Capture a fresh photo from a USB webcam (verified working)",
    )

    text_group = parser.add_mutually_exclusive_group()
    text_group.add_argument("--text", help="Typed question about the image")
    text_group.add_argument("--listen", action="store_true", help="Speak the question (offline STT)")

    parser.add_argument("--save-capture", default="/tmp/rdk_capture.jpg", help="Where to save a --camera capture")
    parser.add_argument("--camera-index", type=int, default=0, help="Which sensor of the GW130W stereo module (0 or 1)")
    parser.add_argument("--speak", action="store_true", help="Speak the answer aloud (offline TTS)")
    parser.add_argument("--api-key", default=None, help="Overrides GEMINI_API_KEY env var")
    parser.add_argument("--gemini-model", default=DEFAULT_GEMINI_MODEL)
    parser.add_argument("--online-timeout", type=float, default=DEFAULT_ONLINE_TIMEOUT)
    parser.add_argument("--force-offline", action="store_true", help="Disable the online backend entirely")
    args = parser.parse_args()

    # Defaults to the USB webcam -- verified working; the MIPI camera (--camera) is still unresolved.
    if not any([args.image, args.camera, args.usb_camera, args.text, args.listen, args.speak]):
        args.usb_camera = True
        args.listen = True
        args.speak = True
    else:
        if not args.image and not args.camera and not args.usb_camera:
            parser.error("one of --image, --camera, or --usb-camera is required")
        if not args.text and not args.listen:
            parser.error("one of --text or --listen is required")

    assistant = HybridVisualAssistant(
        api_key=(None if args.force_offline else args.api_key),
        gemini_model=args.gemini_model,
        online_timeout=args.online_timeout,
    )
    if args.force_offline:
        assistant.network.stop()  # never report online, so ask() always takes the offline path

    try:
        if args.listen:
            print("Listening for your question... (Ctrl+C to cancel)", file=sys.stderr)
            question = assistant.listen_question()
            if not question:
                print("No speech recognized.", file=sys.stderr)
                sys.exit(1)
            print(f"Heard: {question}", file=sys.stderr)
        else:
            question = args.text

        if args.usb_camera:
            print("Capturing photo from USB camera...", file=sys.stderr)
            from camera import capture_usb_photo

            image_path = capture_usb_photo(args.save_capture)
        elif args.camera:
            print(f"Capturing photo from GW130W (camera_index={args.camera_index})...", file=sys.stderr)
            from camera import capture_photo

            image_path = capture_photo(args.save_capture, camera_index=args.camera_index)
        else:
            image_path = args.image

        print("Thinking...", file=sys.stderr)
        answer = assistant.ask(image_path, question)
        print(answer)

        if args.speak:
            assistant.speak(answer)
    finally:
        assistant.close()


if __name__ == "__main__":
    main()
