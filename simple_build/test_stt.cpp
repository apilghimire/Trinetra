// Simple standalone test for STT functionality
// Compile with: clang++ -std=c++14 -o test_stt test_stt.cpp

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <cstdint>
#include <chrono>
#include <sstream>

// Simplified audio input structure
struct SimpleAudioInput {
    std::vector<int16_t> samples;
    int sample_rate = 16000;
    int channels = 1;
    float duration_seconds = 0.0f;
};

// Simple STT result
struct SimpleSTTResult {
    std::string text;
    bool success = false;
    float confidence = 0.0f;
    long processing_time_ms = 0;
};

class SimpleWhisperSTT {
public:
    SimpleWhisperSTT(const std::string& model = "tiny") : model_(model) {}
    
    bool Initialize() {
        std::cout << "Initializing Whisper STT (" << model_ << ")..." << std::endl;
        
        // Check if whisper is available
        if (std::system("python3 -c 'import whisper' 2>/dev/null") != 0) {
            std::cout << "Installing OpenAI Whisper..." << std::endl;
            if (std::system("pip3 install -q openai-whisper") != 0) {
                std::cerr << "Failed to install whisper" << std::endl;
                return false;
            }
        }
        
        std::cout << "✓ Whisper STT initialized" << std::endl;
        return true;
    }
    
    SimpleSTTResult ProcessAudio(const SimpleAudioInput& audio) {
        SimpleSTTResult result;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        if (audio.samples.empty()) {
            return result;
        }
        
        // Save audio to temporary file
        std::string temp_file = "/tmp/whisper_test_" + std::to_string(std::rand()) + ".wav";
        
        if (!SaveAudioToWAV(audio, temp_file)) {
            return result;
        }
        
        // Run whisper
        std::ostringstream cmd;
        cmd << "python3 -c \""
            << "import whisper; "
            << "import sys; "
            << "try: "
            << "  model = whisper.load_model('" << model_ << "'); "
            << "  result = model.transcribe('" << temp_file << "'); "
            << "  print(result['text'].strip()) "
            << "except Exception as e: "
            << "  print('ERROR: ' + str(e), file=sys.stderr); "
            << "  sys.exit(1)"
            << "\" 2>/dev/null";
        
        FILE* pipe = popen(cmd.str().c_str(), "r");
        if (!pipe) {
            std::remove(temp_file.c_str());
            return result;
        }
        
        char buffer[4096];
        std::string output;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }
        
        int return_code = pclose(pipe);
        std::remove(temp_file.c_str());
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        result.processing_time_ms = duration.count();
        
        if (return_code == 0 && !output.empty()) {
            // Remove trailing newline
            if (output.back() == '\n') output.pop_back();
            
            result.text = output;
            result.success = true;
            result.confidence = 0.9f;
        }
        
        return result;
    }
    
private:
    std::string model_;
    
    bool SaveAudioToWAV(const SimpleAudioInput& audio, const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        
        // Write WAV header
        const int data_size = audio.samples.size() * sizeof(int16_t);
        const int file_size = 36 + data_size;
        const int byte_rate = audio.sample_rate * audio.channels * 2;
        
        file.write("RIFF", 4);
        file.write(reinterpret_cast<const char*>(&file_size), 4);
        file.write("WAVE", 4);
        file.write("fmt ", 4);
        const int fmt_size = 16;
        file.write(reinterpret_cast<const char*>(&fmt_size), 4);
        const short audio_format = 1;
        file.write(reinterpret_cast<const char*>(&audio_format), 2);
        const short channels = audio.channels;
        file.write(reinterpret_cast<const char*>(&channels), 2);
        file.write(reinterpret_cast<const char*>(&audio.sample_rate), 4);
        file.write(reinterpret_cast<const char*>(&byte_rate), 4);
        const short block_align = audio.channels * 2;
        file.write(reinterpret_cast<const char*>(&block_align), 2);
        const short bits_per_sample = 16;
        file.write(reinterpret_cast<const char*>(&bits_per_sample), 2);
        file.write("data", 4);
        file.write(reinterpret_cast<const char*>(&data_size), 4);
        file.write(reinterpret_cast<const char*>(audio.samples.data()), data_size);
        
        file.close();
        return true;
    }
};

SimpleAudioInput RecordAudio(float duration) {
    SimpleAudioInput audio;
    
    std::cout << "🎤 Recording " << duration << " seconds... speak now!" << std::endl;
    
    std::string temp_file = "/tmp/record_test_" + std::to_string(std::rand()) + ".wav";
    
    // Try different recording methods
    std::string cmd;
    if (std::system("which sox > /dev/null 2>&1") == 0) {
        cmd = "rec -q -t wav -c 1 -r 16000 -b 16 \"" + temp_file + "\" trim 0 " + std::to_string(duration);
    } else if (std::system("which ffmpeg > /dev/null 2>&1") == 0) {
        cmd = "ffmpeg -y -loglevel quiet -f avfoundation -i \":0\" -t " + 
              std::to_string(duration) + " -ar 16000 -ac 1 -sample_fmt s16 \"" + temp_file + "\" 2>/dev/null";
    } else {
        std::cerr << "No recording tool available (install sox or ffmpeg)" << std::endl;
        return audio;
    }
    
    if (std::system(cmd.c_str()) == 0) {
        // Read the recorded file
        std::ifstream file(temp_file, std::ios::binary);
        if (file.is_open()) {
            // Skip WAV header
            file.seekg(44);
            
            // Read audio data
            int16_t sample;
            while (file.read(reinterpret_cast<char*>(&sample), sizeof(sample))) {
                audio.samples.push_back(sample);
            }
            
            audio.sample_rate = 16000;
            audio.channels = 1;
            audio.duration_seconds = duration;
            
            file.close();
            std::cout << "✓ Recorded " << audio.samples.size() << " samples" << std::endl;
        }
        
        std::remove(temp_file.c_str());
    } else {
        std::cerr << "Recording failed" << std::endl;
    }
    
    return audio;
}

void TestTTS() {
    std::cout << "\n🗣️  Testing TTS..." << std::endl;
    
    std::vector<std::string> test_phrases = {
        "Hello, this is a test of the text to speech system.",
        "The quick brown fox jumps over the lazy dog.",
        "Testing speech synthesis with different sentences."
    };
    
    for (const auto& phrase : test_phrases) {
        std::cout << "Speaking: " << phrase << std::endl;
        std::string cmd = "say \"" + phrase + "\"";
        std::system(cmd.c_str());
    }
}

void TestSTT() {
    std::cout << "\n🎤 Testing STT..." << std::endl;
    
    SimpleWhisperSTT stt("tiny");
    if (!stt.Initialize()) {
        std::cerr << "Failed to initialize STT" << std::endl;
        return;
    }
    
    std::cout << "\nWhen ready, press Enter and speak a test phrase..." << std::endl;
    std::cin.get();
    
    SimpleAudioInput audio = RecordAudio(5.0f);
    
    if (!audio.samples.empty()) {
        std::cout << "🤖 Processing speech..." << std::endl;
        
        SimpleSTTResult result = stt.ProcessAudio(audio);
        
        if (result.success) {
            std::cout << "✓ Recognition result: \"" << result.text << "\"" << std::endl;
            std::cout << "✓ Processing time: " << result.processing_time_ms << "ms" << std::endl;
            
            // Echo back with TTS
            std::cout << "🗣️  Speaking back: " << result.text << std::endl;
            std::string cmd = "say \"You said: " + result.text + "\"";
            std::system(cmd.c_str());
        } else {
            std::cout << "❌ Recognition failed" << std::endl;
        }
    } else {
        std::cout << "❌ No audio recorded" << std::endl;
    }
}

void HandleCommand(const std::string& command);

void VoiceAssistantDemo() {
    std::cout << "\n🤖 Voice Assistant Demo" << std::endl;
    std::cout << "Say 'Hello computer' followed by a command" << std::endl;
    std::cout << "Commands: hello, time, weather, stop" << std::endl;
    std::cout << "Press Ctrl+C to exit\n" << std::endl;
    
    SimpleWhisperSTT stt("tiny");
    if (!stt.Initialize()) {
        std::cerr << "Failed to initialize STT" << std::endl;
        return;
    }
    
    std::system("say \"Voice assistant ready. Say hello computer to wake me up.\"");
    
    while (true) {
        std::cout << "🎤 Listening... (3 seconds)" << std::endl;
        
        SimpleAudioInput audio = RecordAudio(3.0f);
        
        if (!audio.samples.empty()) {
            SimpleSTTResult result = stt.ProcessAudio(audio);
            
            if (result.success && !result.text.empty()) {
                std::cout << "👂 Heard: \"" << result.text << "\"" << std::endl;
                
                // Simple wake word detection
                std::string lower_text = result.text;
                std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
                
                if (lower_text.find("hello computer") != std::string::npos) {
                    std::cout << "🤖 Wake word detected!" << std::endl;
                    
                    // Extract command
                    size_t pos = lower_text.find("hello computer");
                    if (pos != std::string::npos) {
                        std::string command = result.text.substr(pos + 14); // Length of "hello computer"
                        
                        // Trim whitespace
                        command.erase(0, command.find_first_not_of(" \t\n\r"));
                        command.erase(command.find_last_not_of(" \t\n\r") + 1);
                        
                        if (!command.empty()) {
                            HandleCommand(command);
                        } else {
                            std::system("say \"Yes, I'm listening. How can I help you?\"");
                        }
                    }
                }
            }
        }
    }
}

void HandleCommand(const std::string& command) {
    std::string lower_command = command;
    std::transform(lower_command.begin(), lower_command.end(), lower_command.begin(), ::tolower);
    
    std::cout << "🤖 Processing command: " << command << std::endl;
    
    if (lower_command.find("hello") != std::string::npos) {
        std::system("say \"Hello! I'm the speech recognition demo system.\"");
    } else if (lower_command.find("time") != std::string::npos) {
        auto now = std::time(nullptr);
        auto* tm = std::localtime(&now);
        char time_str[100];
        std::strftime(time_str, sizeof(time_str), "The current time is %I:%M %p", tm);
        std::string cmd = "say \"" + std::string(time_str) + "\"";
        std::system(cmd.c_str());
    } else if (lower_command.find("weather") != std::string::npos) {
        std::system("say \"I don't have access to weather information yet, but it's always a good day for robotics!\"");
    } else if (lower_command.find("stop") != std::string::npos || lower_command.find("quit") != std::string::npos) {
        std::system("say \"Goodbye!\"");
        exit(0);
    } else {
        std::string response = "say \"I heard you say: " + command + ". I'm still learning how to respond to different commands.\"";
        std::system(response.c_str());
    }
}

int main() {
    std::cout << "=" << std::string(50, '=') << std::endl;
    std::cout << "🤖 Simple TTS+STT Test Program" << std::endl;
    std::cout << "=" << std::string(50, '=') << std::endl;
    
    std::cout << "\nChoose a test:" << std::endl;
    std::cout << "1. Test TTS (Text-to-Speech)" << std::endl;
    std::cout << "2. Test STT (Speech-to-Text)" << std::endl;
    std::cout << "3. Voice Assistant Demo" << std::endl;
    std::cout << "0. Exit" << std::endl;
    
    int choice;
    std::cout << "\nEnter choice: ";
    std::cin >> choice;
    std::cin.ignore(); // Consume newline
    
    switch (choice) {
        case 1:
            TestTTS();
            break;
        case 2:
            TestSTT();
            break;
        case 3:
            VoiceAssistantDemo();
            break;
        case 0:
            std::cout << "Goodbye!" << std::endl;
            break;
        default:
            std::cout << "Invalid choice" << std::endl;
            break;
    }
    
    return 0;
}
