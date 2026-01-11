"""
Wake-word detector built on top of the offline STT module.

Waits for each complete STT chunk (a finalized utterance, i.e. Vosk has
decided the speaker paused) and checks it for the wake word "hello" (as a
whole word, case-insensitive). On a match, the full recognized sentence is
reported/returned rather than just the trigger word, e.g. saying
"hello turn on the lights" reports the whole sentence, not just "hello".
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Callable, Optional

sys.path.insert(0, str(Path(__file__).resolve().parent))

from stt import DEFAULT_MODEL_DIR, OfflineSTT, Transcript

WAKE_WORD = "hello"


def contains_wake_word(text: str, wake_word: str = WAKE_WORD) -> bool:
    """True if wake_word appears as a whole word in text (case-insensitive)."""
    pattern = rf"\b{re.escape(wake_word)}\b"
    return re.search(pattern, text, re.IGNORECASE) is not None


def listen_for_wake_word(
    stt: OfflineSTT,
    wake_word: str = WAKE_WORD,
    on_detect: Optional[Callable[[Transcript], None]] = None,
    check_partial: bool = False,
) -> Transcript:
    """Block until a full STT chunk containing wake_word is heard.

    By default only finalized transcripts (complete utterances) are checked,
    so the returned Transcript.text is the whole sentence that was spoken,
    not just the wake word fragment. Pass check_partial=True to also react
    to in-progress partial results (faster, but text may be incomplete).
    """
    for transcript in stt.listen():
        if not transcript.is_final and not check_partial:
            continue
        if contains_wake_word(transcript.text, wake_word):
            if on_detect:
                on_detect(transcript)
            return transcript
    raise RuntimeError("STT stream ended without detecting the wake word")


def main() -> None:
    parser = argparse.ArgumentParser(description="Wake-word ('hello') detector using offline STT")
    parser.add_argument("--model", default=str(DEFAULT_MODEL_DIR), help="Path to Vosk model dir")
    parser.add_argument("--device", default=None, help="Input audio device index or name")
    parser.add_argument("--wake-word", default=WAKE_WORD, help="Word to listen for")
    parser.add_argument(
        "--partial",
        action="store_true",
        help="Also react to in-progress partial results, not just full final chunks",
    )
    parser.add_argument(
        "--once",
        action="store_true",
        help="Exit after the first detection (default: keep listening)",
    )
    args = parser.parse_args()

    device = args.device
    if device is not None and device.isdigit():
        device = int(device)

    stt = OfflineSTT(model_dir=args.model, device=device)

    def report(transcript: Transcript) -> None:
        print(f"[wakeword] '{args.wake_word}' detected. Full sentence: \"{transcript.text}\"")

    print(f"Listening for wake word '{args.wake_word}'... (Ctrl+C to stop)", file=sys.stderr)
    try:
        while True:
            listen_for_wake_word(
                stt,
                wake_word=args.wake_word,
                on_detect=report,
                check_partial=args.partial,
            )
            if args.once:
                break
    except KeyboardInterrupt:
        print("\nStopped.", file=sys.stderr)


if __name__ == "__main__":
    main()
