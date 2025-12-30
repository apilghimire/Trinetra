// Copyright (c) 2024, Extended Hobot TTS with Speech-to-Text.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HOBOT_TTS_MICROPHONE_RECORDER_H_
#define HOBOT_TTS_MICROPHONE_RECORDER_H_

#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>
#include <cstdint>

namespace hobot_tts {

// Audio input structure (reused from stt_engine.h)
struct AudioInput {
  std::vector<int16_t> samples;
  int sample_rate = 16000;
  int channels = 1;
  float duration_seconds = 0.0f;
};

// Recording information structure
struct RecordingInfo {
  int sample_rate;
  int channels;
  int bits_per_sample;
  bool vad_enabled;
  float silence_threshold;
  float min_speech_duration;
  float max_silence_duration;
  bool is_recording;
};

// Callback type for continuous recording
using RecordingCallback = std::function<void(const AudioInput&)>;

/**
 * @class MicrophoneRecorder
 * @brief Cross-platform microphone recording with Voice Activity Detection (VAD)
 * 
 * This class provides microphone recording capabilities with built-in VAD for
 * automatic speech detection and silence removal. Supports both single-shot
 * and continuous recording modes.
 */
class MicrophoneRecorder {
public:
  /**
   * @brief Constructor
   */
  MicrophoneRecorder();
  
  /**
   * @brief Destructor - stops any active recordings
   */
  ~MicrophoneRecorder();
  
  /**
   * @brief Initialize the microphone recorder
   * @return true if initialization successful
   */
  bool Initialize();
  
  /**
   * @brief Set recording parameters
   * @param sample_rate Sample rate in Hz (default: 16000)
   * @param channels Number of channels (default: 1 for mono)
   * @param bits_per_sample Bit depth (default: 16)
   */
  void SetRecordingParameters(int sample_rate = 16000, int channels = 1, int bits_per_sample = 16);
  
  /**
   * @brief Configure Voice Activity Detection parameters
   * @param enable Enable/disable VAD
   * @param silence_threshold RMS threshold for silence detection (0.0-1.0)
   * @param min_speech_duration Minimum duration of speech to be valid (seconds)
   * @param max_silence_duration Maximum silence before ending recording (seconds)
   */
  void SetVADParameters(bool enable = true, float silence_threshold = 0.01f, 
                       float min_speech_duration = 0.5f, float max_silence_duration = 1.0f);
  
  /**
   * @brief Record audio for a fixed duration
   * @param duration Duration in seconds
   * @return AudioInput containing recorded samples
   */
  AudioInput RecordAudio(float duration);
  
  /**
   * @brief Record audio using Voice Activity Detection
   * @return AudioInput containing recorded speech (automatically trimmed)
   */
  AudioInput RecordWithVAD();
  
  /**
   * @brief Start continuous recording with callback
   * @param callback Function called for each detected speech segment
   * @return true if recording started successfully
   */
  bool StartContinuousRecording(RecordingCallback callback);
  
  /**
   * @brief Stop continuous recording
   */
  void StopContinuousRecording();
  
  /**
   * @brief Stop all recording activities
   * @return true if stopped successfully
   */
  bool StopRecording();
  
  /**
   * @brief Check if currently recording
   * @return true if recording is active
   */
  bool IsRecording() const;
  
  /**
   * @brief Get current recording configuration
   * @return RecordingInfo structure with current settings
   */
  RecordingInfo GetRecordingInfo() const;

private:
  // Recording parameters
  int sample_rate_;
  int channels_;
  int bits_per_sample_;
  
  // VAD parameters
  bool vad_enabled_;
  float silence_threshold_;
  float min_speech_duration_;
  float max_silence_duration_;
  float buffer_duration_;
  
  // State
  bool initialized_ = false;
  std::atomic<bool> recording_;
  std::thread recording_thread_;
  RecordingCallback recording_callback_;
  
  /**
   * @brief Check if recording capability is available
   * @return true if recording tools are available
   */
  bool CheckRecordingCapability();
  
  /**
   * @brief Read WAV file and convert to AudioInput
   * @param filename Path to WAV file
   * @return AudioInput with file contents
   */
  AudioInput ReadWAVFile(const std::string& filename);
  
  /**
   * @brief Calculate RMS energy of audio samples
   * @param samples Audio samples
   * @return RMS energy value (0.0-1.0)
   */
  float CalculateRMS(const std::vector<int16_t>& samples);
};

}  // namespace hobot_tts

#endif  // HOBOT_TTS_MICROPHONE_RECORDER_H_
