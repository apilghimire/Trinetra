"""
Wake-word-gated live visual assistant for the RDK X5.

Pipeline: listen continuously -> on a fully-recognized STT chunk containing
the wake word ("hello"), take that full sentence as the question -> capture
a live photo from the USB webcam -> answer via the hybrid VLM (online Gemini
Flash when connected, offline SmolVLM2+BPU otherwise, see hybrid_assistant.py)
-> speak the answer aloud (offline Piper TTS).

Reuses stt/wakeword.py for wake-word detection (one shared OfflineSTT
instance -- the onboard mic has no dmix/dsnoop, so only one thing may hold
it open at a time) and vision/hybrid_assistant.py for the capture+ask+speak
leg.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Optional

VISION_DIR = Path(__file__).resolve().parent
PROJECT_DIR = VISION_DIR.parent
sys.path.insert(0, str(VISION_DIR))
sys.path.insert(0, str(PROJECT_DIR / "stt"))
sys.path.insert(0, str(PROJECT_DIR / "tts"))

from hybrid_assistant import DEFAULT_ONLINE_TIMEOUT, HybridVisualAssistant  # noqa: E402
from online_client import DEFAULT_MODEL as DEFAULT_GEMINI_MODEL  # noqa: E402
from stt import DEFAULT_MODEL_DIR, OfflineSTT  # noqa: E402
from wakeword import WAKE_WORD, listen_for_wake_word  # noqa: E402

DEFAULT_QUESTION = "What do you see in front of me?"


def extract_question(sentence: str, wake_word: str) -> str:
    """Strip the wake word out of the heard sentence to get the actual question.

    "hello what is in front of me" -> "what is in front of me". If nothing
    is left over (the user just said the wake word alone), fall back to a
    generic description prompt.
    """
    remainder = re.sub(rf"\b{re.escape(wake_word)}\b", "", sentence, count=1, flags=re.IGNORECASE)
    remainder = remainder.strip(" ,.?!")
    return remainder if remainder else DEFAULT_QUESTION


def run(
    assistant: HybridVisualAssistant,
    stt: OfflineSTT,
    wake_word: str = WAKE_WORD,
    save_capture: str | Path = "/tmp/usb_capture.jpg",
    once: bool = False,
) -> None:
    print(f"Listening for wake word '{wake_word}'... (Ctrl+C to stop)", file=sys.stderr)
    while True:
        transcript = listen_for_wake_word(stt, wake_word=wake_word)
        question = extract_question(transcript.text, wake_word)
        print(f"[wake] heard: \"{transcript.text}\" -> question: \"{question}\"", file=sys.stderr)

        print("Capturing photo from USB camera...", file=sys.stderr)
        answer = assistant.ask_usb_camera(question, save_capture=save_capture)
        print(answer)
        assistant.speak(answer)

        if once:
            break


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Wake-word-gated live visual assistant: say 'hello' + a question, "
        "get a live photo answered by the hybrid VLM and spoken back."
    )
    parser.add_argument("--wake-word", default=WAKE_WORD, help="Wake word to listen for")
    parser.add_argument("--stt-model", default=str(DEFAULT_MODEL_DIR), help="Path to Vosk model dir")
    parser.add_argument("--device", default=None, help="Input audio device index or name")
    parser.add_argument("--save-capture", default="/tmp/usb_capture.jpg", help="Where to save each capture")
    parser.add_argument("--once", action="store_true", help="Exit after answering one query")
    parser.add_argument("--api-key", default=None, help="Overrides GEMINI_API_KEY env var")
    parser.add_argument("--gemini-model", default=DEFAULT_GEMINI_MODEL)
    parser.add_argument("--online-timeout", type=float, default=DEFAULT_ONLINE_TIMEOUT)
    parser.add_argument("--force-offline", action="store_true", help="Disable the online VLM backend entirely")
    args = parser.parse_args()

    device = args.device
    if device is not None and device.isdigit():
        device = int(device)

    stt = OfflineSTT(model_dir=args.stt_model, device=device)
    assistant = HybridVisualAssistant(
        api_key=(None if args.force_offline else args.api_key),
        gemini_model=args.gemini_model,
        online_timeout=args.online_timeout,
    )
    if args.force_offline:
        assistant.network.stop()

    try:
        run(
            assistant,
            stt,
            wake_word=args.wake_word,
            save_capture=args.save_capture,
            once=args.once,
        )
    except KeyboardInterrupt:
        print("\nStopped.", file=sys.stderr)
    finally:
        assistant.close()


if __name__ == "__main__":
    main()
