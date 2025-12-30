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

#ifndef HOBOT_TTS_MACOS_AUDIO_MANAGER_H_
#define HOBOT_TTS_MACOS_AUDIO_MANAGER_H_

#include <string>
#include <vector>
#include <memory>

#ifdef __APPLE__
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#endif

namespace hobot_tts {

struct AudioDevice {
  std::string name;
  std::string id;
  int channels;
  int sample_rate;
};

class AudioManager {
 public:
  static std::unique_ptr<AudioManager> Create(const std::string& device_name = "default");
  virtual ~AudioManager() = default;

  // Pure virtual functions
  virtual bool Initialize() = 0;
  virtual bool PlayAudio(const std::vector<int16_t>& audio_data, int sample_rate, int channels) = 0;
  virtual std::vector<AudioDevice> GetAvailableDevices() = 0;
  virtual bool SetOutputDevice(const std::string& device_name) = 0;
  virtual void Cleanup() = 0;

 protected:
  AudioManager() = default;
};

#ifdef __APPLE__
class CoreAudioManager : public AudioManager {
 public:
  CoreAudioManager(const std::string& device_name = "default");
  ~CoreAudioManager() override;

  bool Initialize() override;
  bool PlayAudio(const std::vector<int16_t>& audio_data, int sample_rate, int channels) override;
  std::vector<AudioDevice> GetAvailableDevices() override;
  bool SetOutputDevice(const std::string& device_name) override;
  void Cleanup() override;

 private:
  std::string device_name_;
  AudioUnit audio_unit_;
  bool initialized_;
  
  // Audio callback data
  struct CallbackData {
    std::vector<int16_t> audio_data;
    size_t current_frame;
    int sample_rate;
    int channels;
    bool finished;
  };
  
  CallbackData callback_data_;
  
  // Static callback function
  static OSStatus AudioCallback(void* inRefCon,
                               AudioUnitRenderActionFlags* ioActionFlags,
                               const AudioTimeStamp* inTimeStamp,
                               UInt32 inBusNumber,
                               UInt32 inNumberFrames,
                               AudioBufferList* ioData);
  
  // Helper methods
  bool SetupAudioUnit();
  void TeardownAudioUnit();
  AudioDeviceID GetDeviceID(const std::string& device_name);
};
#endif

class SimpleAudioManager : public AudioManager {
 public:
  SimpleAudioManager(const std::string& device_name = "default");
  ~SimpleAudioManager() override;

  bool Initialize() override;
  bool PlayAudio(const std::vector<int16_t>& audio_data, int sample_rate, int channels) override;
  std::vector<AudioDevice> GetAvailableDevices() override;
  bool SetOutputDevice(const std::string& device_name) override;
  void Cleanup() override;

 private:
  std::string device_name_;
  bool initialized_;
  
  // Uses system commands like afplay on macOS
  bool PlayWithSystemCommand(const std::vector<int16_t>& audio_data, int sample_rate, int channels);
  std::string CreateTempWavFile(const std::vector<int16_t>& audio_data, int sample_rate, int channels);
};

}  // namespace hobot_tts

#endif  // HOBOT_TTS_MACOS_AUDIO_MANAGER_H_
