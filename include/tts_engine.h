// Copyright (c) 2024, Adapted from D-Robotics Hobot TTS.
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

#ifndef HOBOT_TTS_MACOS_TTS_ENGINE_H_
#define HOBOT_TTS_MACOS_TTS_ENGINE_H_

#include <string>
#include <vector>
#include <memory>

namespace hobot_tts {

enum class TTSEngineType {
  SAY,        // macOS built-in say command
  FESTIVAL,   // Festival TTS
  ESPEAK      // eSpeak TTS
};

struct AudioData {
  std::vector<int16_t> samples;
  int sample_rate;
  int channels;
  
  AudioData(int rate = 16000, int ch = 1) 
    : sample_rate(rate), channels(ch) {}
};

class TTSEngine {
 public:
  static std::unique_ptr<TTSEngine> Create(TTSEngineType type, const std::string& voice = "");
  virtual ~TTSEngine() = default;

  // Pure virtual functions
  virtual bool Initialize() = 0;
  virtual bool Synthesize(const std::string& text, AudioData& audio_data) = 0;
  virtual void SetVoice(const std::string& voice_name) = 0;
  virtual std::vector<std::string> GetAvailableVoices() = 0;
  virtual void Cleanup() = 0;

 protected:
  TTSEngine() = default;
};

class SayEngine : public TTSEngine {
 public:
  SayEngine(const std::string& voice = "");
  ~SayEngine() override;

  bool Initialize() override;
  bool Synthesize(const std::string& text, AudioData& audio_data) override;
  void SetVoice(const std::string& voice_name) override;
  std::vector<std::string> GetAvailableVoices() override;
  void Cleanup() override;

 private:
  std::string voice_name_;
  bool initialized_;
  
  // Helper methods
  std::string EscapeText(const std::string& text);
  bool ConvertAudioFile(const std::string& input_file, AudioData& audio_data);
};

class FestivalEngine : public TTSEngine {
 public:
  FestivalEngine(const std::string& voice = "");
  ~FestivalEngine() override;

  bool Initialize() override;
  bool Synthesize(const std::string& text, AudioData& audio_data) override;
  void SetVoice(const std::string& voice_name) override;
  std::vector<std::string> GetAvailableVoices() override;
  void Cleanup() override;

 private:
  std::string voice_name_;
  bool initialized_;
  
  std::string EscapeText(const std::string& text);
  bool ConvertAudioFile(const std::string& input_file, AudioData& audio_data);
};

class ESpeakEngine : public TTSEngine {
 public:
  ESpeakEngine(const std::string& voice = "");
  ~ESpeakEngine() override;

  bool Initialize() override;
  bool Synthesize(const std::string& text, AudioData& audio_data) override;
  void SetVoice(const std::string& voice_name) override;
  std::vector<std::string> GetAvailableVoices() override;
  void Cleanup() override;

 private:
  std::string voice_name_;
  bool initialized_;
  
  std::string EscapeText(const std::string& text);
  bool ConvertAudioFile(const std::string& input_file, AudioData& audio_data);
};

}  // namespace hobot_tts

#endif  // HOBOT_TTS_MACOS_TTS_ENGINE_H_
