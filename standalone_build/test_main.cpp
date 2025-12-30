#include <iostream>
#include <string>
#include <vector>
#include "../src/tts_engine.cpp"
#include "../src/audio_manager.cpp"

int main() {
    std::cout << "=== Standalone Hobot TTS Test ===" << std::endl;
    
    // Test TTS Engine
    auto tts_engine = hobot_tts::TTSEngine::Create(hobot_tts::TTSEngineType::SAY, "");
    if (!tts_engine) {
        std::cerr << "Failed to create TTS engine" << std::endl;
        return 1;
    }
    
    if (!tts_engine->Initialize()) {
        std::cerr << "Failed to initialize TTS engine" << std::endl;
        return 1;
    }
    
    // Test Audio Manager
    auto audio_manager = hobot_tts::AudioManager::Create("default");
    if (!audio_manager) {
        std::cerr << "Failed to create audio manager" << std::endl;
        return 1;
    }
    
    if (!audio_manager->Initialize()) {
        std::cerr << "Failed to initialize audio manager" << std::endl;
        return 1;
    }
    
    // Test text synthesis and playback
    std::vector<std::string> test_texts = {
        "Hello, this is a standalone test of the Hobot TTS system.",
        "Testing text to speech without ROS2 dependencies.",
        "The system is working correctly!"
    };
    
    for (const auto& text : test_texts) {
        std::cout << "Processing: " << text << std::endl;
        
        hobot_tts::AudioData audio_data;
        if (tts_engine->Synthesize(text, audio_data)) {
            std::cout << "Generated " << audio_data.samples.size() << " audio samples" << std::endl;
            
            if (audio_manager->PlayAudio(audio_data.samples, audio_data.sample_rate, audio_data.channels)) {
                std::cout << "✓ Audio played successfully" << std::endl;
            } else {
                std::cout << "✗ Audio playback failed" << std::endl;
            }
        } else {
            std::cout << "✗ Text synthesis failed" << std::endl;
        }
        
        std::cout << "Press Enter to continue..." << std::endl;
        std::cin.get();
    }
    
    // Show available voices
    std::cout << "Available voices:" << std::endl;
    auto voices = tts_engine->GetAvailableVoices();
    for (const auto& voice : voices) {
        std::cout << "  - " << voice << std::endl;
    }
    
    tts_engine->Cleanup();
    audio_manager->Cleanup();
    
    std::cout << "Test completed successfully!" << std::endl;
    return 0;
}
