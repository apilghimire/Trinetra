# Offline visual assistant (RDK X5)

Image (file, USB webcam, or GW130W/GS130W MIPI capture) + text (typed or spoken) -> **SmolVLM2-256M**
(GGUF text decoder + BPU-accelerated vision encoder) -> a text answer.
Fully offline at inference time — no cloud calls for vision, language, or speech.

Built on top of the [offline STT module](../stt/README.md) (`stt/stt.py`, Vosk) for
the voice-input path and the [offline TTS module](../tts/README.md) (`tts/tts.py`,
Piper) for spoken answers.

## Layout

- `llama.cpp_bpu/` — llama.cpp (tag `b4749`) patched with D-Robotics' BPU CLI
  tools (`build/bin/llama-smolvlm-bpu-cli`) — **this is what `assistant.py` uses**
- `llama.cpp/` — mainline llama.cpp (latest, `build/bin/llama-mtmd-cli`) — CPU-only
  fallback, no longer the default (see "Model history" below)
- `camera.py` — two independent capture backends:
  - `USBCamera`/`capture_usb_photo()` — plain UVC webcam via OpenCV/V4L2. **Verified
    working** with a real live capture.
  - `RDKCamera`/`capture_photo()` — GW130W/GS130W MIPI stereo module (D-Robotics
    `hobot_vio` SDK), one 2D frame from **one** sensor (`camera_index`, default 0).
    **Unverified** — extensive real-hardware debugging (sensor identity, disabled
    `camera_power0`/`1` device-tree nodes, missing GDC calibration) never got it
    working; see "Known limitations"
- `banknote_detector.py` — `BanknoteDetector`: offline YOLO object detector
  (Hugging Face `Imbatmann/nepali-banknote-models`, exported to NCNN) that
  finds and classifies Nepali banknotes by denomination; `assistant.py`'s
  `ask()` runs this first and folds its findings into the SmolVLM2 prompt as
  grounding context — see "Offline banknote detection" below
- `assistant.py` — `VisualAssistant` class + CLI tying camera/STT/TTS/llama.cpp together.
  **Fully offline, always** — use this directly if you need that guarantee.
- `network_monitor.py` — `NetworkMonitor`: background thread polling internet
  connectivity (TCP connect to 8.8.8.8/1.1.1.1:53 every 5s)
- `online_client.py` — `GeminiLiveClient`: persistent WebSocket connection to
  Gemini's Multimodal Live API (`BidiGenerateContent`) for near-instant online
  image+text queries
- `hybrid_assistant.py` — `HybridVisualAssistant` class + CLI: online Gemini
  Flash when there's a network, offline SmolVLM2 always loaded as backup —
  see "Online/offline hybrid mode" below
- `blind_assistant.py` — `BlindAssistant` class + CLI: continuous listen ->
  see -> answer -> speak loop around `HybridVisualAssistant`, for hands-free
  use by a blind/low-vision user — see "Continuous assistant for blind/
  low-vision users" below
- `../models/smolvlm2-bpu/` — `SmolVLM2-256M-Video-Instruct-Q8_0.gguf` (~175MB) +
  `rdkx5/SigLip_int16_SmolVLM2_256M_Instruct_MLP_C1_UP_X5.bin` (~186MB, BPU vision encoder)
- `../models/gemma-3-4b-it-gguf/`, `../models/internvl2_5-1b-bpu/` — earlier
  attempts, kept on disk, no longer used by default (see below)

## Usage

```bash
# The full voice system: listen for a question, capture a photo, answer, speak it.
# Zero arguments = --usb-camera --listen --speak (USB webcam, the verified path).
python3 assistant.py

# Typed question about an existing image
python3 assistant.py --image photo.jpg --text "What is in this image?"

# Typed question about a fresh photo from the USB webcam
python3 assistant.py --usb-camera --text "What is in front of the robot?"

# Spoken question (offline STT), USB webcam, speak the answer
python3 assistant.py --usb-camera --listen --speak

# GW130W/GS130W MIPI camera instead -- UNVERIFIED on real hardware, see below
python3 assistant.py --camera --camera-index 1 --text "What is in this image?"
```

As a library:

```python
from assistant import VisualAssistant

va = VisualAssistant()
print(va.ask("photo.jpg", "Describe this scene."))
print(va.ask_usb_camera("What color is the object in front?"))   # verified working
print(va.ask_camera("What color is the object in front?"))       # MIPI, unverified, camera_index=0
```

**English only** — SmolVLM2 doesn't support other languages.

## Online/offline hybrid mode

`hybrid_assistant.py` wraps `assistant.py`'s offline `VisualAssistant` with an
online path: Gemini Flash over the official Live API (`google-genai` SDK,
WebSocket under the hood), used whenever a network is up, with automatic
fallback to the offline model.

`GEMINI_API_KEY` must be a standard Gemini Developer API key from
[aistudio.google.com/apikey](https://aistudio.google.com/apikey) (format
`AIzaSy...`, 39 chars) — not an ephemeral token from the AI Studio "Stream
Realtime" playground (format `AQ....`), which is scoped to one playground
session/model and gets rejected.

The default model (`online_client.py`'s `DEFAULT_MODEL`) is
`gemini-3.1-flash-live-preview` — the current general-purpose (not
audio-dialog-only, not translation-only) Flash model supporting
`bidiGenerateContent`. If it starts getting rejected later, list what your
key actually has access to:
```python
from google import genai
for m in genai.Client(api_key="...").models.list():
    if "bidiGenerateContent" in (m.supported_actions or []):
        print(m.name)
```
Live-API Flash models are voice-first: they reject a `TEXT`-only
`response_modalities` request, so `online_client.py` requests `AUDIO` output
with `output_audio_transcription` enabled and reads the transcript — the
audio itself is discarded, only the transcribed text is used.

```bash
export GEMINI_API_KEY=your-key-here
python3 hybrid_assistant.py --image photo.jpeg --text "What is in this image?"

# Same voice pipeline as assistant.py, but online-first per query
python3 hybrid_assistant.py

# Skip the online backend entirely (equivalent to plain assistant.py, but
# through the hybrid CLI/class)
python3 hybrid_assistant.py --image photo.jpeg --text "..." --force-offline
```

How it works:
- **Both backends load at startup.** The offline SmolVLM2 model loads
  unconditionally (same ~9s load as plain `assistant.py`) so it's ready the
  instant it's needed. The Gemini Live WebSocket client starts its own
  background asyncio loop and opens a persistent connection immediately if a
  network is already up.
- **`NetworkMonitor` polls connectivity continuously** (a cheap TCP connect to
  8.8.8.8/1.1.1.1:53 every 5s, not a full HTTP request) in a daemon thread.
  On a down→up transition it reconnects the Gemini Live WebSocket right away,
  so the connection is typically already established by the time the next
  query comes in.
- **Every `ask()` call is online-first:** if the network is up and the
  WebSocket is connected, it sends the image+question over that persistent
  connection and returns Gemini's answer. On any failure — timeout, dropped
  connection, missing `GEMINI_API_KEY` — it transparently falls back to the
  offline SmolVLM2 model for that same query.
- No conversation memory across queries in either mode (matches plain
  `assistant.py`'s one-shot-per-query design); the persistent WebSocket is
  reused for connection speed, not for multi-turn context.

**Verified end-to-end** with a real `GEMINI_API_KEY` and `photo.jpeg`: both
`online_client.py` standalone and the full `hybrid_assistant.py` CLI
correctly connected to the live Gemini Live API, answered from the actual
image content, and printed `[hybrid] answered online (Gemini Flash)`. The
offline fallback path was separately verified end-to-end (no key configured)
against the actual on-device SmolVLM2/BPU pipeline. Not yet verified: the
`NetworkMonitor`-driven reconnect-on-network-loss path (killing/restoring
the network mid-session) and behavior under concurrent queries.

## Offline banknote detection

`assistant.py`'s offline path (SmolVLM2-256M) hallucinates on Nepali
banknotes -- confirmed twice in testing, wrong country ("Indian banknotes")
and invented denominations that don't appear in the image (a "$200 note", a
"200 rupees" bill). `banknote_detector.py` fixes this specifically: a YOLO
object detector fine-tuned on Nepali banknotes
([`Imbatmann/nepali-banknote-models`](https://huggingface.co/Imbatmann/nepali-banknote-models)
on Hugging Face) runs first, and its findings (e.g. "found these Nepali
banknote denominations: 10, 50, 10") get prepended to the question sent to
SmolVLM2 as grounding context. `VisualAssistant.ask()` does this
automatically (`detect_banknotes=True` by default); pass `detect_banknotes=False`
to skip it for non-banknote images.

**One-time setup required, with network access**, before this works offline:
```bash
python3 -c "import banknote_detector; banknote_detector.bootstrap()"
```
This downloads `best.pt` (~113MB) and exports it to NCNN (~9 minutes,
CPU-bound, one-time). After that, `BanknoteDetector` never touches the
network — see the `local_files_only=True` note in `_ensure_model()`. This
was a real bug caught during testing, not a hypothetical: without it,
`hf_hub_download()` does a HEAD request to Hugging Face on *every single
query* to check the cached file is current, even when it already is --
which stalled one query to 2m30s wall-clock over a slow/rate-limited
connection, entirely defeating the point of an "offline" path.

Why NCNN and not raw PyTorch: `best.pt` is large for CPU inference (58.8M
params, ~195 GFLOPs -- roughly YOLO11x scale; there's no smaller variant in
the HF repo). Raw PyTorch CPU inference measured ~42s/image on this board;
NCNN export (`model.export(format="ncnn")`, Tencent's ARM-optimized
inference format) cut that to ~5s/image in isolation with a warm model
already loaded in a long-running process.

**Be aware of the gap between that ~5s number and real end-to-end latency**,
both found during testing:
- **Cold start**: every query is a fresh process (matches this whole
  module's one-shot-per-query design), so "~5s" (warm, already-loaded model)
  is not what a real query costs. A fresh process paying model load +
  inference once measured **~26-30s** for the detector alone, on top of
  SmolVLM2's own ~13-30s -- so **~40-60s realistic total** for one grounded
  offline query, not ~18s.
- **System load matters a lot, and can dominate everything else.** On this
  development machine (which, unusually for a "deployed" scenario, runs a
  full desktop session -- Xfce, Firefox, a VNC server, and the Claude
  Desktop app itself, one of whose renderer processes was observed pegged
  near 99% CPU for over an hour) load average was 4.69/6.99/6.34 on 8 cores
  during testing, and a single grounded query was measured taking **2m50s**
  wall-clock (vs. 12+ minutes of aggregate CPU time across threads) under
  that contention. The exact same code, same image, same question completed
  in the ~40-60s range when the system was less loaded. If deploying this
  for real hands-free use, prefer a headless/minimal environment (no desktop
  GUI competing for the same 8 CPU cores) for consistent latency -- this
  isn't a code bug, it's the board's fixed CPU budget being shared with
  whatever else happens to be running on it.

**Verified working end-to-end**, both the fix and its cost:
- Detector alone against `note.jpeg`: correctly found denominations (10, 50,
  10, 5, 50, 5, 10, 20 -- some duplicates/misses vs. the 7 actual notes,
  expected for a detector, not a perfect count).
- Full grounded `assistant.py --image note.jpeg --text "..."` run: answer
  correctly said **"Nepali banknote denominations"** (the country
  hallucination from the ungrounded baseline is gone) and listed real
  detected values (10, 50, 10) -- though the response was cut off mid-
  sentence by the default `-n 200` token generation limit; raise
  `--n-predict` if you need the full description for a busier image.

## Continuous assistant for blind/low-vision users

`blind_assistant.py` is the actual product: a hands-free loop, not a one-shot
CLI. It listens continuously, and for each question it hears it captures a
fresh photo, answers (online-first via `HybridVisualAssistant`, offline
fallback), and speaks the answer — then goes straight back to listening for
the next question. Runs until Ctrl+C.

```bash
export GEMINI_API_KEY=your-key-here
python3 blind_assistant.py    # USB webcam by default (verified working)

# Force fully offline (no Gemini calls at all)
python3 blind_assistant.py --force-offline

# GW130W/GS130W MIPI camera instead (UNVERIFIED), other sensor, longer listen timeout
python3 blind_assistant.py --camera --camera-index 1 --listen-timeout 30
```

Design notes:
- **Never dies on a single failure.** Each stage of one question/answer cycle
  (listening, camera capture, answering) is individually caught; a failure is
  spoken aloud ("Sorry, I couldn't use the camera.") and logged, and the loop
  goes right back to listening — an assistant a blind user depends on to see
  should never silently exit on one bad frame or one dropped connection.
- **Speaks a short "Let me look." cue** before each answer (the offline path
  takes several seconds — see "Performance" below — and a blind user has no
  visual "thinking..." indicator to fall back on, unlike the plain
  `assistant.py`/`hybrid_assistant.py` CLIs which only print that to stderr).
  Disable with `--no-announce`.
- **`assistant.listen_question()` is called fresh each cycle** (no persistent
  mic stream across questions, same as `assistant.py`/`hybrid_assistant.py`);
  each cycle waits up to `--listen-timeout` seconds (default 20s) for speech
  before looping back to listen again with no question asked.

**Verified for real, with the actual on-device pipeline** (not mocked, aside
from substituting a static test photo for the camera capture step — see the
camera caveat below): ran `BlindAssistant.handle_one_question()` end-to-end
with a simulated spoken question, real photo, real inference, and real TTS
playback, for both backends —
online (~40s: listen -> "Let me look" spoken -> live Gemini query -> answer
spoken) and offline (~55s: same flow through the on-device SmolVLM2/BPU
pipeline instead, which is inherently slower — see "Performance"). Both
completed cleanly with no crash and no hang.

**Not verified:** live spoken input through a real microphone (mic capture
confirmed to be receiving real signal in this environment — non-zero RMS
after the ReSpeaker HAT device-tree overlay was applied — but no one spoke
into it during testing) and `BlindAssistant`'s own `_capture()` against a
*real, live* camera specifically (the run above substituted a static test
photo). The underlying USB capture call it now defaults to
(`capture_usb_photo()`) *is* separately verified working end-to-end via
`assistant.py` — real photo, real SmolVLM2/BPU inference, real Piper
playback — just not yet re-run through `BlindAssistant`'s own loop with a
live frame. The MIPI camera path (`--camera`) remains fully unverified;
`_capture()`'s failure path, which speaks "Sorry, I couldn't use the
camera.", was not itself exercised either way.

## Performance (measured on this board)

- Model load: ~9s
- Vision encoding (on the **BPU**): ~1.1s
- Text generation: ~2-3s (256M-parameter decoder, ~10 tok/s on CPU)
- **Total: ~13 seconds per query**

## Model history: why SmolVLM2 + BPU, not Gemma-3 or InternVL

This went through three iterations:

1. **Gemma-3-4B-it** (CPU-only, mainline llama.cpp, `llama.cpp/`). Requested repo
   `google/gemma-3-4b-it-GGUF` is gated on Hugging Face; used the ungated mirror
   `unsloth/gemma-3-4b-it-GGUF` instead (same weights). Worked, but **~7 minutes
   per query** — CPU-only vision encoding for a 4B model dominates. Model files
   still on disk at `../models/gemma-3-4b-it-gguf/`.

2. **InternVL2.5-1B on the BPU** — investigated per your request. D-Robotics
   publishes a hybrid pipeline (`D-Robotics/InternVL2_5-1B-GGUF-BPU`): a
   BPU-compiled InternViT vision encoder + a tiny Qwen2.5-0.5B GGUF text decoder,
   run via a patched llama.cpp fork
   ([zixi01chen/llama.cpp_vlm_bpu](https://github.com/zixi01chen/llama.cpp_vlm_bpu),
   tag `b4749`). Built and downloaded successfully, but **the vision encoder
   (390MB) doesn't fit this board's reserved BPU memory pool** — `/proc/meminfo`
   shows `CmaTotal: 391168 kB` (~382MB total), and that pool is shared with the
   camera ISP. Fails with `hbrtErrorBPUMemAllocFail` / `ion_alloc: buffer create
   error[-12]` (ENOMEM). This is a board boot-config limit (`cma=` kernel
   parameter), not a software bug — growing it requires a boot-config change,
   which wasn't made (would also affect camera ISP memory; flagged to you but
   not done). Files kept at `../models/internvl2_5-1b-bpu/` and
   `llama.cpp_bpu/build/bin/llama-intern2vl-bpu-cli` in case that pool is grown
   later — the binary works, it's purely the memory ceiling that blocks it.

3. **SmolVLM2-256M on the BPU** (current default) — same D-Robotics hybrid
   approach, but its SigLip vision encoder is only 186MB, comfortably inside the
   382MB pool. `git clone` and build were mostly reused from step 2, just needed
   the `llama-smolvlm-bpu-cli` target. Confirmed working: BPU vision encoding at
   ~1.1s instead of ~5 minutes on CPU. Traded model capability (256M vs 1-4B
   parameters, English-only) for a **~30x** end-to-end speedup.

If you outgrow SmolVLM2's quality and want to revisit InternVL/Gemma-3, either
grow the CMA pool yourself and retry `llama.cpp_bpu/build/bin/llama-intern2vl-bpu-cli`,
or point `assistant.py` back at `llama.cpp/build/bin/llama-mtmd-cli` +
`models/gemma-3-4b-it-gguf/` (both still present) and accept the CPU-only latency.

## Known limitations

- **USB webcam capture is verified working** (`USBCamera`/`capture_usb_photo()`
  in `camera.py`) — real live capture confirmed, and a full real-hardware
  `assistant.py --usb-camera --text ... --speak` run completed end-to-end:
  real photo, real SmolVLM2/BPU inference, real Piper playback. This is why
  it's the default across `assistant.py`, `hybrid_assistant.py`, and
  `blind_assistant.py`.

- **Offline SmolVLM2 is unreliable for text-reading/OCR-style questions and
  small factual details — online should be treated as the primary path for
  these, not just a latency optimization.** Tested against `note.jpeg` (a
  photo of Nepali banknotes, fanned out, with printed Devanagari text and
  numerals):
  - Online (Gemini Flash) got it right: correctly identified Nepali currency,
    read the Devanagari text (bank name, "रूपैयाँ पाँच" / "five rupees"), and
    listed denominations accurately.
  - Offline (SmolVLM2-256M) failed three different ways depending on the
    prompt: asked to read the text, it degenerated into a wall of repeated
    "0" digits; asked to describe the scene, it answered fluently but
    **hallucinated** ("Indian banknotes", a "$200 note" — wrong country,
    invented denomination, invented currency symbol, none of which appear in
    the image); asked a direct simple question ("what color are these
    papers?"), it deflected with a non-answer.
  - Takeaway: a 256M-parameter model doesn't have the capacity for reliable
    small-text/fine-detail recognition, and — worse than an obvious failure —
    it can produce fluent, confident, wrong answers. For an accessibility
    tool where a blind user acts on the answer (e.g. trusting a stated
    currency value), that's a real risk, not just a quality gap.
  - **Fixed for Nepali banknotes specifically** via `banknote_detector.py` --
    see the "Offline banknote detection" section below. Online should still
    be treated as the more general-purpose/reliable path for arbitrary
    text-reading; the banknote detector only covers this one domain.

- **The GW130W/GS130W MIPI stereo camera never got working, despite a long
  real-hardware debugging session.** Summary, in case you pick this back up:
  - The board is D-Robotics' RDK X5 GS130W (dual SC132GS global-shutter
    sensors, 80mm baseline, confirmed via the vendor's own tutorial PDF —
    "GW130W" was a misremembering of the actual product name).
  - Camera electrically connects (real I2C ACKs on both buses: i2c-4 at
    `0x33`/`0x50`, i2c-6 at `0x32`), but sensor identification consistently
    fails — every chip-ID readback returns `0x00`/garbage, with a recurring
    `"mipi mclk is not configed"` warning.
  - Root cause traced to `/sys/firmware/devicetree/base/soc/camera_power{0,1}`
    both being `status = "disabled"` — the sensors are unpowered, not just
    unclocked. No exposed toggle for this was found in `srpi-config`,
    `/boot/overlays`, or the installed packages.
  - The three RPi-compatible overlays (`ov5647`/`imx219`/`imx477`, selectable
    via `srpi-config`) target a *different physical connector*
    (`i2c@341d0000`/`csi@3d060000`) than the GS130W's actual ports
    (`vcon@0`/`vcon@2`, i2c-6/i2c-4) — that path can't apply to this sensor
    regardless of configuration.
  - The official command (`ros2 launch mipi_cam mipi_cam_dual_channel.launch.py`,
    confirmed via the vendor PDF as the complete, correct invocation with no
    extra parameters documented as required) gets further than any raw
    `srcampy` call — it reads real EEPROM data (`EEPROM FLAG: SZYGSJKJ`,
    left/right AWB calibration blocks, all `0xFFFF`/unprogrammed) — but then
    fails later, in `create_and_run_vflow`. Likely culprit: `mipi_gdc_enable`
    defaults to `True` with `mipi_gdc_bin_file` defaulting to an *empty
    string* — GDC bin files for this sensor do exist
    (`/app/multimedia_samples/vp_sensors/gdc_bin/sc132gs_1088X1280_gdc.bin`),
    just never passed. Untested: `mipi_gdc_enable:=False`, or passing that
    bin file plus `mipi_camera_calibration_file_path` pointed at
    `/opt/tros/humble/lib/mipi_cam/config/SC132gs_dual_calibration.yaml`.
  - **Important vendor warning, not yet acted on**: the tutorial PDF states
    hot-plugging this camera while the board is powered can damage it. The
    board stayed powered throughout this entire investigation. If picking
    this back up, power off, verify the cable seating, and cold-boot before
    trying again.
  - A Raspberry Pi Zero (OV5647) camera was also tried on the separate
    `cam1`/RPi-compatible connector — genuinely untested as of this writing;
    the `/boot/config.txt` overlay line for it was written but not yet
    confirmed working after reboot.
- **Which `camera_index` (0 or 1) maps to which physical sensor of the GW130W/
  GS130W stereo module is unverified**, for the same reason. `camera_index=0`
  is the default/first guess; if `--camera` ever reads the wrong lens, try
  `--camera-index 1`.
- Only one query at a time; each `ask()` call spawns a fresh CLI process (no
  persistent server/session), so there's no conversation memory between calls.
- `llama-smolvlm-bpu-cli` leaks a couple of raw stdout prints from the BPU
  vendor runtime and a hardcoded debug line ahead of the actual answer;
  `assistant.py` handles this by splitting on the `=== Generating response ===`
  marker rather than trusting all of stdout — see the comment in `ask()`.
