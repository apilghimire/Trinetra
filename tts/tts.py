"""
Offline text-to-speech module for the RDK X5, using Piper (neural TTS, ONNX).

Fully offline: no network calls, no cloud TTS API. Piper is a small VITS-based
neural model -- genuinely natural-sounding, unlike rule-based engines like
espeak-ng. See README.md for the latency tradeoff (ONNX model load is the
dominant per-call cost, not synthesis itself).
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

TTS_DIR = Path(__file__).resolve().parent
PROJECT_DIR = TTS_DIR.parent

DEFAULT_MODEL_PATH = PROJECT_DIR / "models" / "piper-voices" / "en" / "en_US" / "lessac" / "medium" / "en_US-lessac-medium.onnx"
DEFAULT_CONFIG_PATH = Path(str(DEFAULT_MODEL_PATH) + ".json")


class OfflineTTS:
    """Speaks text or synthesizes it to a WAV file, via Piper."""

    def __init__(
        self,
        model_path: Path = DEFAULT_MODEL_PATH,
        config_path: Path = DEFAULT_CONFIG_PATH,
    ) -> None:
        for label, path in [("Piper model", model_path), ("Piper config", config_path)]:
            if not path.exists():
                raise FileNotFoundError(f"{label} not found at {path}")
        self.model_path = model_path
        self.config_path = config_path

    def synthesize(self, text: str, out_path: str | Path) -> Path:
        """Synthesize text to a WAV file."""
        out_path = Path(out_path)
        subprocess.run(
            ["piper", "-m", str(self.model_path), "-c", str(self.config_path), "-f", str(out_path)],
            input=text,
            text=True,
            check=True,
            stderr=subprocess.DEVNULL,  # suppress Piper's harmless "GPU device discovery failed" warning
        )
        return out_path

    def speak(self, text: str) -> None:
        """Synthesize and play text immediately through the default audio device."""
        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
            wav_path = Path(f.name)
        try:
            self.synthesize(text, wav_path)
            subprocess.run(["aplay", "-q", str(wav_path)], check=True)
        finally:
            wav_path.unlink(missing_ok=True)


def main() -> None:
    parser = argparse.ArgumentParser(description="Offline TTS (Piper) for RDK X5")
    parser.add_argument("text", nargs="?", help="Text to speak (reads stdin if omitted)")
    parser.add_argument("--model", type=Path, default=DEFAULT_MODEL_PATH, help="Path to a Piper .onnx voice model")
    parser.add_argument("--save", help="Write to this WAV file instead of playing")
    args = parser.parse_args()

    text = args.text if args.text is not None else sys.stdin.read().strip()
    if not text:
        print("No text given.", file=sys.stderr)
        sys.exit(1)

    config_path = Path(str(args.model) + ".json")
    tts = OfflineTTS(model_path=args.model, config_path=config_path)
    if args.save:
        path = tts.synthesize(text, args.save)
        print(f"Saved: {path}")
    else:
        tts.speak(text)


if __name__ == "__main__":
    main()
