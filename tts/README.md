# Offline text-to-speech (RDK X5)

Fully offline TTS using **Piper** — a small neural (VITS-based) model, run locally
via ONNX Runtime. No network calls, no cloud API.

## Why Piper

Genuinely natural-sounding, unlike rule-based engines (espeak-ng, which this module
used to use — see git history / earlier session notes if you want to compare).
Piper is purpose-built for exactly this class of hardware (originally targeted at
Raspberry Pi for offline voice assistants), ships a prebuilt `manylinux ... aarch64`
wheel (no compilation needed on this board), and has many voices/quality tiers
across languages if you want to trade speed for even more natural output.

## Setup

```bash
pip3 install -r requirements.txt
```

Voice model (already downloaded to `../models/piper-voices/en/en_US/lessac/medium/`):

```bash
python3 -c "
from huggingface_hub import hf_hub_download
for f in ['en/en_US/lessac/medium/en_US-lessac-medium.onnx',
          'en/en_US/lessac/medium/en_US-lessac-medium.onnx.json']:
    hf_hub_download('rhasspy/piper-voices', f, local_dir='../models/piper-voices')
"
```

Browse other voices/languages/quality tiers at
https://huggingface.co/rhasspy/piper-voices — swap in a different `.onnx` +
`.onnx.json` pair via `OfflineTTS(model_path=...)` or `--model`.

## Usage

```bash
# Speak text immediately
python3 tts.py "Hello, this is a test."

# Save to a WAV file instead of playing
python3 tts.py "Hello" --save out.wav

# Read text from stdin
echo "Hello" | python3 tts.py

# Use a different voice model
python3 tts.py "Bonjour" --model /path/to/fr_FR-voice-medium.onnx
```

As a library:

```python
from tts import OfflineTTS

tts = OfflineTTS()
tts.speak("The answer is ready.")
tts.synthesize("Save this instead.", "out.wav")
```

## Performance (measured on this board)

**~9-12s per call**, almost entirely ONNX Runtime model-load overhead — a 13-word
sentence and a 3-word sentence both took roughly the same total time (~9-10s).
Actual synthesis + playback is fast once the model is loaded; the cost is that
`tts.py` spawns a fresh `piper` process (and therefore a fresh model load) on every
call, same pattern/limitation as the vision assistant before its persistent-server
fix (see [../vision/README.md](../vision/README.md)). If this latency matters for
your use case, the same fix applies here: a small resident Piper server that loads
once and serves repeated `speak()` calls over a socket, instead of a fresh process
per call. Not built yet — ask if you want it.

## Known limitation: shared audio device

Same constraint documented in [../stt/README.md](../stt/README.md): this board's
`duplex-audio` device (hw:0,0) has no dmix/dsnoop, so it can't be opened by two
processes at once. If something else holds the mic open continuously (e.g. the
`stt` module's `listen()`, or a leftover ASR process), `speak()`'s `aplay` call
will hang waiting for the device rather than failing immediately — check `ps aux`
for a stuck consumer of `plughw:0,0`/`hw:0,0` if playback seems to hang.
