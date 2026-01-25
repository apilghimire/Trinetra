"""
Offline visual assistant for the RDK X5: image (file or live GW130W capture)
+ text (typed or spoken via the offline STT module) -> SmolVLM2-256M
(GGUF text decoder + BPU-accelerated SigLip vision encoder) -> a text answer.

Everything runs locally: no cloud calls for vision, language, or speech.

Model choice: the vision encoder runs on this board's NPU (BPU) via a
D-Robotics-compiled .bin model, cutting per-query latency from ~7 minutes
(Gemma-3-4B-it / InternVL, CPU-only) to ~13 seconds. The trade-off is a much
smaller, English-only model -- see vision/README.md for the full comparison
and why InternVL2.5-1B's BPU vision encoder doesn't fit this board (it needs
390MB+ and the board's CMA pool is only 382MB, shared with the camera ISP).
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path
from typing import Optional

VISION_DIR = Path(__file__).resolve().parent
PROJECT_DIR = VISION_DIR.parent

BPU_CLI = VISION_DIR / "llama.cpp_bpu" / "build" / "bin" / "llama-smolvlm-bpu-cli"
MODEL_PATH = PROJECT_DIR / "models" / "smolvlm2-bpu" / "SmolVLM2-256M-Video-Instruct-Q8_0.gguf"
MMPROJ_PATH = (
    PROJECT_DIR / "models" / "smolvlm2-bpu" / "rdkx5" / "SigLip_int16_SmolVLM2_256M_Instruct_MLP_C1_UP_X5.bin"
)

DEFAULT_CTX_SIZE = 4096
DEFAULT_BATCH_SIZE = 512
DEFAULT_N_PREDICT = 200
DEFAULT_TEMP = 0.5

# Sibling module reuse: stt/stt.py and tts/tts.py.
sys.path.insert(0, str(PROJECT_DIR / "stt"))
sys.path.insert(0, str(PROJECT_DIR / "tts"))


class VisualAssistant:
    """Runs SmolVLM2-256M locally: BPU vision encoder + CPU (llama.cpp) text decoder."""

    def __init__(
        self,
        bpu_cli: Path = BPU_CLI,
        model_path: Path = MODEL_PATH,
        mmproj_path: Path = MMPROJ_PATH,
        ctx_size: int = DEFAULT_CTX_SIZE,
        batch_size: int = DEFAULT_BATCH_SIZE,
        n_predict: int = DEFAULT_N_PREDICT,
        temp: float = DEFAULT_TEMP,
    ) -> None:
        for label, path in [("llama-smolvlm-bpu-cli binary", bpu_cli), ("model", model_path), ("mmproj", mmproj_path)]:
            if not path.exists():
                raise FileNotFoundError(f"{label} not found at {path}")
        self.bpu_cli = bpu_cli
        self.model_path = model_path
        self.mmproj_path = mmproj_path
        self.ctx_size = ctx_size
        self.batch_size = batch_size
        self.n_predict = n_predict
        self.temp = temp
        self._stt = None  # lazily constructed, only needed for --listen
        self._tts = None  # lazily constructed, only needed for --speak
        self._banknote_detector = None  # lazily constructed, only needed if detect_banknotes=True

    def ask(self, image_path: str | Path, question: str, detect_banknotes: bool = True) -> str:
        """Run one image+text query through SmolVLM2. Returns the answer text.

        Vision encoding runs on the BPU (~1s); text generation runs on CPU
        (~2-3s for a short answer). Total observed latency: ~13s including
        model load. English only -- see D-Robotics' model card.

        detect_banknotes: run the offline Nepali banknote detector
        (banknote_detector.py) first and fold any findings into the prompt as
        grounding context (~5s added, NCNN-exported YOLO). SmolVLM2 alone
        hallucinates on this task (see vision/README.md's Known
        Limitations) -- this exists to fix that, not as an optimization, so
        it defaults on. Set False to skip it (e.g. for non-banknote images
        where the extra ~5s buys nothing).
        """
        image_path = Path(image_path)
        if not image_path.exists():
            raise FileNotFoundError(f"Image not found: {image_path}")

        if detect_banknotes:
            if self._banknote_detector is None:
                from banknote_detector import BanknoteDetector

                self._banknote_detector = BanknoteDetector()
            try:
                context = self._banknote_detector.describe(image_path)
            except Exception as exc:
                print(f"[assistant] banknote detection failed, continuing without it: {exc}", file=sys.stderr)
                context = None
            if context:
                question = f"{context}\n\n{question}"

        cmd = [
            str(self.bpu_cli),
            "-m", str(self.model_path),
            "--mmproj", str(self.mmproj_path),
            "-c", str(self.ctx_size),
            "-b", str(self.batch_size),
            "-ub", str(self.batch_size),
            "--image", str(image_path),
            "-p", question,
            "-n", str(self.n_predict),
            "--temp", str(self.temp),
        ]
        # Most diagnostic/progress/perf lines (via LOG_INF) go to stderr,
        # separately from the actual model response (via LOG, no level
        # prefix) on stdout -- see common/log.cpp: GGML_LOG_LEVEL_NONE goes
        # to stdout, every other level goes to stderr. BUT this BPU-patched
        # CLI also has a couple of hardcoded stdout prints outside that
        # mechanism (the vendor BPU runtime's own startup banner, plus a
        # debug "reize height/width" line in smolvlm-bpu-cli.cpp) that leak
        # onto stdout ahead of the real answer. The answer is reliably
        # preceded by a "=== Generating response ===" marker (itself LOG()'d
        # right before token output starts), so split on that instead of
        # trusting all of stdout.
        result = subprocess.run(
            cmd,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if result.returncode != 0:
            raise RuntimeError(
                f"llama-smolvlm-bpu-cli exited with code {result.returncode}:\n{result.stderr[-4000:]}"
            )
        marker = "=== Generating response ==="
        idx = result.stdout.find(marker)
        if idx == -1:
            return result.stdout.strip()
        return result.stdout[idx + len(marker):].strip()

    def ask_camera(
        self, question: str, save_capture: str | Path = "/tmp/rdk_capture.jpg", camera_index: int = 0
    ) -> str:
        """Capture a fresh photo from one sensor of the GW130W stereo module, then ask about it.

        UNVERIFIED on real hardware -- see vision/README.md. Prefer ask_usb_camera()
        unless/until the MIPI camera bring-up is resolved.
        """
        from camera import capture_photo  # local import: only needed on-device with hobot_vio

        image_path = capture_photo(save_capture, camera_index=camera_index)
        return self.ask(image_path, question)

    def ask_usb_camera(self, question: str, save_capture: str | Path = "/tmp/usb_capture.jpg") -> str:
        """Capture a fresh photo from the USB webcam, then ask about it. Verified working."""
        from camera import capture_usb_photo

        image_path = capture_usb_photo(save_capture)
        return self.ask(image_path, question)

    def listen_question(self, timeout: float = 15.0) -> Optional[str]:
        """Capture one spoken question via the offline STT module (Vosk)."""
        if self._stt is None:
            from stt import OfflineSTT  # stt/stt.py

            self._stt = OfflineSTT()

        import time

        gen = self._stt.listen()
        start = time.monotonic()
        text = None
        try:
            for transcript in gen:
                if transcript.is_final and transcript.text:
                    text = transcript.text
                    break
                if time.monotonic() - start > timeout:
                    break
        finally:
            gen.close()
        return text

    def speak(self, text: str) -> None:
        """Speak text aloud via the offline TTS module (espeak-ng)."""
        if self._tts is None:
            from tts import OfflineTTS  # tts/tts.py

            self._tts = OfflineTTS()
        self._tts.speak(text)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Offline visual assistant (SmolVLM2 + BPU) for RDK X5. "
        "Run with no arguments for the full voice system: listen for a question, "
        "capture a photo, answer, and speak the answer."
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
    parser.add_argument(
        "--camera-index", type=int, default=0,
        help="Which sensor of the GW130W stereo module to use (0 or 1, default 0)",
    )
    parser.add_argument("--speak", action="store_true", help="Speak the answer aloud (offline TTS)")
    parser.add_argument("--ctx-size", type=int, default=DEFAULT_CTX_SIZE)
    parser.add_argument("--n-predict", type=int, default=DEFAULT_N_PREDICT)
    parser.add_argument("--temp", type=float, default=DEFAULT_TEMP)
    args = parser.parse_args()

    # Run with zero arguments -> the full voice system: listen, capture, answer, speak.
    # Defaults to the USB webcam, since that's the verified-working camera path;
    # the MIPI camera (--camera) is still unresolved on real hardware.
    if not any([args.image, args.camera, args.usb_camera, args.text, args.listen, args.speak]):
        args.usb_camera = True
        args.listen = True
        args.speak = True
    else:
        if not args.image and not args.camera and not args.usb_camera:
            parser.error("one of --image, --camera, or --usb-camera is required")
        if not args.text and not args.listen:
            parser.error("one of --text or --listen is required")

    assistant = VisualAssistant(ctx_size=args.ctx_size, n_predict=args.n_predict, temp=args.temp)

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


if __name__ == "__main__":
    main()
