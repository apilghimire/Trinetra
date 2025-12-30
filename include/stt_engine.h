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

#ifndef HOBOT_STT_ENGINE_H_
#define HOBOT_STT_ENGINE_H_

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace hobot_tts {

enum class STTEngineType {
  WHISPER_TINY,    // Fastest, lowest resource usage (~39 MB)
  WHISPER_BASE,    // Good balance (~74 MB)
  WHISPER_SMALL,   // Better accuracy (~244 MB)
  WHISPER_API,     // OpenAI Whisper API (requires API key)
  MACOS_DICTATION, // macOS built-in (if available)
  GOOGLE_API       // Cloud-based (requires internet)
};

struct AudioInput {
  std::vector<int16_t> samples;
  int sample_rate;
  int channels;
  float duration_seconds;
  
  AudioInput(int rate = 16000, int ch = 1) 
    : sample_rate(rate), channels(ch), duration_seconds(0.0f) {}
};

struct STTResult {
  std::string text;
  float confidence;
  bool success;
  std::string language;
  float processing_time_ms;
  
  STTResult() : confidence(0.0f), success(false), processing_time_ms(0.0f) {}
};

// Callback for real-time STT results
using STTCallback = std::function<void(const STTResult&)>;

class STTEngine {
 public:
  // Factory methods
  static std::unique_ptr<STTEngine> Create(STTEngineType type, const std::string& model_path = "");
  static std::unique_ptr<STTEngine> CreateWithAPIKey(const std::string& api_key, const std::string& model = "whisper-1");
  virtual ~STTEngine() = default;

  // Core STT functions
  virtual bool Initialize() = 0;
  virtual STTResult ProcessAudio(const AudioInput& audio) = 0;
  virtual bool StartRealTimeRecognition(STTCallback callback) = 0;
  virtual void StopRealTimeRecognition() = 0;
  
  // Configuration
  virtual void SetLanguage(const std::string& language) = 0;
  virtual std::vector<std::string> GetSupportedLanguages() = 0;
  virtual void SetVAD(bool enable) = 0; // Voice Activity Detection
  
  // Utility
  virtual bool IsRealTimeCapable() const = 0;
  virtual std::string GetModelInfo() const = 0;
  virtual void Cleanup() = 0;

 protected:
  STTEngine() = default;
};

// Whisper-based STT Engine (Recommended)
class WhisperSTTEngine : public STTEngine {
 public:
  WhisperSTTEngine(STTEngineType model_type, const std::string& model_path = "");
  ~WhisperSTTEngine() override;

  bool Initialize() override;
  STTResult ProcessAudio(const AudioInput& audio) override;
  bool StartRealTimeRecognition(STTCallback callback) override;
  void StopRealTimeRecognition() override;
  
  void SetLanguage(const std::string& language) override;
  std::vector<std::string> GetSupportedLanguages() override;
  void SetVAD(bool enable) override;
  
  bool IsRealTimeCapable() const override { return true; }
  std::string GetModelInfo() const override;
  void Cleanup() override;

 private:
  STTEngineType model_type_;
  std::string model_path_;
  std::string language_;
  bool vad_enabled_;
  bool initialized_;
  bool real_time_active_;
  
  // Whisper model handle (we'll use whisper.cpp)
  void* whisper_ctx_;
  
  // Real-time processing
  std::thread real_time_thread_;
  STTCallback real_time_callback_;
  
  // Helper methods
  bool DownloadModel();
  bool LoadModel();
  std::string ModelTypeToString() const;
  std::string GetModelURL() const;
  AudioInput RecordAudio(float duration_seconds = 3.0f);
  std::vector<float> ConvertToFloat(const std::vector<int16_t>& samples);
};

// OpenAI Whisper API STT Engine
class WhisperAPIEngine : public STTEngine {
 public:
  explicit WhisperAPIEngine(const std::string& api_key, const std::string& model = "whisper-1");
  ~WhisperAPIEngine() override;

  bool Initialize() override;
  STTResult ProcessAudio(const AudioInput& audio) override;
  bool StartRealTimeRecognition(STTCallback callback) override;
  void StopRealTimeRecognition() override;
  
  void SetLanguage(const std::string& language) override;
  std::vector<std::string> GetSupportedLanguages() override;
  void SetVAD(bool enable) override;
  
  bool IsRealTimeCapable() const override { return true; }
  std::string GetModelInfo() const override { return "OpenAI Whisper API (" + model_ + ")"; }
  void Cleanup() override;

 private:
  std::string api_key_;
  std::string model_;
  std::string language_;
  bool vad_enabled_;
  bool initialized_;
  bool real_time_active_;
  
  // Real-time processing
  std::thread real_time_thread_;
  STTCallback real_time_callback_;
  
  // Helper methods
  std::string SaveAudioToFile(const AudioInput& audio, const std::string& format = "mp3");
  std::string MakeAPIRequest(const std::string& audio_file);
  std::string ParseAPIResponse(const std::string& response);
  AudioInput RecordAudio(float duration_seconds = 3.0f);
};

// macOS Dictation STT Engine
class MacOSDictationEngine : public STTEngine {
 public:
  MacOSDictationEngine();
  ~MacOSDictationEngine() override;

  bool Initialize() override;
  STTResult ProcessAudio(const AudioInput& audio) override;
  bool StartRealTimeRecognition(STTCallback callback) override;
  void StopRealTimeRecognition() override;
  
  void SetLanguage(const std::string& language) override;
  std::vector<std::string> GetSupportedLanguages() override;
  void SetVAD(bool enable) override;
  
  bool IsRealTimeCapable() const override { return false; }
  std::string GetModelInfo() const override { return "macOS Built-in Dictation"; }
  void Cleanup() override;

 private:
  bool initialized_;
  std::string language_;
  
  // Helper methods
  bool IsDictationEnabled();
  std::string SaveAudioToFile(const AudioInput& audio);
  std::string ProcessWithDictation(const std::string& audio_file);
};

// Simple microphone recorder for real-time STT
class MicrophoneRecorder {
 public:
  MicrophoneRecorder(int sample_rate = 16000, int channels = 1);
  ~MicrophoneRecorder();
  
  bool Initialize();
  bool StartRecording();
  void StopRecording();
  AudioInput GetAudioBuffer();
  void ClearBuffer();
  
  // Callback for continuous recording
  void SetAudioCallback(std::function<void(const AudioInput&)> callback);
  
 private:
  int sample_rate_;
  int channels_;
  bool recording_;
  bool initialized_;
  
  std::vector<int16_t> audio_buffer_;
  std::mutex buffer_mutex_;
  std::thread recording_thread_;
  std::function<void(const AudioInput&)> audio_callback_;
  
  void RecordingLoop();
  
#ifdef __APPLE__
  void* audio_unit_; // AudioUnit handle
  bool SetupAudioUnit();
  void TeardownAudioUnit();
  static OSStatus RecordingCallback(void* inRefCon,
                                   AudioUnitRenderActionFlags* ioActionFlags,
                                   const AudioTimeStamp* inTimeStamp,
                                   UInt32 inBusNumber,
                                   UInt32 inNumberFrames,
                                   AudioBufferList* ioData);
#endif
};

}  // namespace hobot_tts

#endif  // HOBOT_STT_ENGINE_H_
