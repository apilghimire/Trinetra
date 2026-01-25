"""
Continuous voice-driven vision assistant for blind/low-vision users, RDK X5.

Runs an indefinite loop: listen for a spoken question (offline STT) -> capture
a fresh photo (USB webcam by default -- verified working; --camera for the
GW130W/GS130W MIPI stereo module, still unresolved on real hardware) -> answer
it (HybridVisualAssistant: online Gemini Flash when there's a network, offline
SmolVLM2 always loaded as backup) -> speak the answer aloud (offline TTS).
Ctrl+C to stop.

This is deliberately crash-resistant: every stage of one question/answer
cycle (listening, capturing, answering) is wrapped so a single failure is
spoken/logged and the loop moves on to listening for the next question,
rather than taking the whole assistant down -- an assistant a blind user is
relying on to see should never silently die on one bad camera frame or one
dropped network connection.
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path
from typing import Optional

VISION_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(VISION_DIR))

from hybrid_assistant import (  # noqa: E402
    DEFAULT_GEMINI_MODEL,
    DEFAULT_ONLINE_TIMEOUT,
    HybridVisualAssistant,
)

DEFAULT_LISTEN_TIMEOUT = 20.0  # seconds to wait for a question before re-listening
DEFAULT_SAVE_CAPTURE = "/tmp/rdk_capture.jpg"
ERROR_BACKOFF = 1.0  # seconds to pause after a listen() failure, to avoid a busy-loop


class BlindAssistant:
    """Continuous listen -> see -> answer -> speak loop around a HybridVisualAssistant."""

    def __init__(
        self,
        assistant: HybridVisualAssistant,
        camera_index: int = 0,
        use_usb_camera: bool = True,
        save_capture: str | Path = DEFAULT_SAVE_CAPTURE,
        listen_timeout: float = DEFAULT_LISTEN_TIMEOUT,
        announce_thinking: bool = True,
    ) -> None:
        self.assistant = assistant
        self.camera_index = camera_index
        # Defaults to the USB webcam -- verified working; the MIPI camera
        # (GW130W/GS130W) is still unresolved on real hardware, see
        # vision/README.md. Set use_usb_camera=False once that's sorted out
        # and you want the stereo module instead.
        self.use_usb_camera = use_usb_camera
        self.save_capture = save_capture
        self.listen_timeout = listen_timeout
        self.announce_thinking = announce_thinking

    def _say(self, text: str) -> None:
        print(text, file=sys.stderr)
        try:
            self.assistant.speak(text)
        except Exception as exc:
            # TTS failing shouldn't take the loop down either -- just log it.
            print(f"[blind-assistant] TTS failed: {exc}", file=sys.stderr)

    def _capture(self) -> Optional[Path]:
        try:
            if self.use_usb_camera:
                from camera import capture_usb_photo

                return capture_usb_photo(self.save_capture)
            else:
                from camera import capture_photo  # local import: only needed on-device with hobot_vio

                return capture_photo(self.save_capture, camera_index=self.camera_index)
        except Exception as exc:
            print(f"[blind-assistant] camera capture failed: {exc}", file=sys.stderr)
            self._say("Sorry, I couldn't use the camera.")
            return None

    def handle_one_question(self) -> None:
        """Listen for one question and answer it. Catches everything; never raises."""
        try:
            question = self.assistant.listen_question(timeout=self.listen_timeout)
        except Exception as exc:
            print(f"[blind-assistant] listening failed: {exc}", file=sys.stderr)
            time.sleep(ERROR_BACKOFF)
            return

        if not question:
            return  # nothing heard within the timeout; go back to listening

        print(f"Heard: {question}", file=sys.stderr)
        if self.announce_thinking:
            self._say("Let me look.")

        image_path = self._capture()
        if image_path is None:
            return

        try:
            answer = self.assistant.ask(image_path, question)
        except Exception as exc:
            print(f"[blind-assistant] ask() failed: {exc}", file=sys.stderr)
            self._say("Sorry, I couldn't answer that.")
            return

        self._say(answer)

    def run(self) -> None:
        """Loop forever: listen, see, answer, speak. Ctrl+C to stop."""
        print("Vision assistant ready. Ask a question any time. Ctrl+C to stop.", file=sys.stderr)
        self._say("Vision assistant ready.")
        try:
            while True:
                self.handle_one_question()
        except KeyboardInterrupt:
            print("\nStopped.", file=sys.stderr)
        finally:
            self.assistant.close()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Continuous voice vision assistant for blind/low-vision users (RDK X5). "
        "Listens for questions, captures a fresh photo, answers (online Gemini Flash if "
        "available, else offline SmolVLM2), and speaks the answer. Runs until Ctrl+C."
    )
    parser.add_argument(
        "--camera", action="store_true",
        help="Use the GW130W/GS130W MIPI camera instead of the USB webcam (UNVERIFIED on real hardware)",
    )
    parser.add_argument("--camera-index", type=int, default=0, help="Which sensor of the GW130W stereo module (0 or 1), only used with --camera")
    parser.add_argument("--save-capture", default=DEFAULT_SAVE_CAPTURE, help="Where each capture is saved")
    parser.add_argument(
        "--listen-timeout", type=float, default=DEFAULT_LISTEN_TIMEOUT,
        help="Seconds to wait for a question before re-listening",
    )
    parser.add_argument("--no-announce", action="store_true", help='Skip speaking "Let me look" while processing')
    parser.add_argument("--api-key", default=None, help="Overrides GEMINI_API_KEY env var")
    parser.add_argument("--gemini-model", default=DEFAULT_GEMINI_MODEL)
    parser.add_argument("--online-timeout", type=float, default=DEFAULT_ONLINE_TIMEOUT)
    parser.add_argument("--force-offline", action="store_true", help="Disable the online backend entirely")
    args = parser.parse_args()

    assistant = HybridVisualAssistant(
        api_key=(None if args.force_offline else args.api_key),
        gemini_model=args.gemini_model,
        online_timeout=args.online_timeout,
    )
    if args.force_offline:
        assistant.network.stop()  # never report online, so ask() always takes the offline path

    BlindAssistant(
        assistant=assistant,
        camera_index=args.camera_index,
        use_usb_camera=not args.camera,
        save_capture=args.save_capture,
        listen_timeout=args.listen_timeout,
        announce_thinking=not args.no_announce,
    ).run()


if __name__ == "__main__":
    main()
