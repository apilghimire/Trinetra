# Offline STT module (RDK X5)

Fully offline speech-to-text using [Vosk](https://alphacephei.com/vosk/), tested on the
D-Robotics RDK X5 (8GB, aarch64, Ubuntu 22.04) onboard mic (ES8326 codec).

No audio or text ever leaves the device — recognition runs entirely on-CPU against a
local acoustic/language model.

## Setup

System packages (already installed on this device):

```bash
sudo apt-get install -y libportaudio2 portaudio19-dev
```

Python packages:

```bash
pip3 install -r requirements.txt
```

Model (already downloaded to `../models/vosk-model-small-en-us`, ~68MB, English small model):

```bash
curl -L -o model.zip https://alphacephei.com/vosk/models/vosk-model-small-en-us-0.15.zip
unzip model.zip -d ../models/
```

Larger/more accurate models are available at https://alphacephei.com/vosk/models — swap
in one of those (e.g. `vosk-model-en-us-0.22`, ~1.8GB) by pointing `--model` / `model_dir`
at the extracted folder if accuracy matters more than footprint.

## Usage

```bash
# Live mic, print final transcripts as they're recognized
python3 stt.py

# Also print in-progress partial results
python3 stt.py --partial

# Transcribe a WAV file instead (must be mono 16-bit PCM)
python3 stt.py --file some_audio.wav

# List audio devices
python3 stt.py --list-devices
```

As a library:

```python
from stt import OfflineSTT

stt = OfflineSTT()  # uses default model + default mic

for transcript in stt.listen():
    if transcript.is_final:
        print(transcript.text)
```

Or with a callback:

```python
stt.run(lambda t: print(t.text) if t.is_final else None)
```

## Hardware notes

- The RDK X5's onboard capture device (`duplex-audio`, hw:0,0) is **fixed at 2 channels**
  — it does not support opening the ALSA device directly in mono. `stt.py` captures
  stereo and downmixes to mono in software before feeding Vosk (which requires mono
  16kHz PCM). If you see the mic capture hang forever, this is almost certainly why.
- The device has **no dmix/dsnoop** configured, so it cannot be opened by two processes
  at once. If you later add TTS/voice output, playing audio (`aplay`, etc.) while this
  module is listening on the same device will fail to start with a PortAudio
  `Wait timed out` error. Either avoid capture+playback overlap, or configure ALSA
  dmix/dsnoop for hw:0,0 first.

## Verified

- `stt.transcribe_file()` round-tripped an espeak-ng synthesized WAV
  ("the quick brown fox jumps over the lazy dog") to an exact match.
- Live mic streaming (`stt.listen()`) ran cleanly for repeated multi-second sessions with
  no crashes or hangs.
