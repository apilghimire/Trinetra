#!/usr/bin/env python3
"""
Demo script for Hobot TTS+STT system
This script demonstrates the voice assistant capabilities
"""

import os
import sys
import time
import subprocess
import tempfile
import threading
from pathlib import Path

def check_dependencies():
    """Check if required dependencies are available"""
    print("🔍 Checking dependencies...")
    
    dependencies = {
        'whisper': 'pip3 install openai-whisper',
        'ffmpeg': 'brew install ffmpeg',
        'sox': 'brew install sox (optional, recommended for better recording)'
    }
    
    # Check Python packages
    try:
        import whisper
        print("✓ OpenAI Whisper found")
    except ImportError:
        print("❌ OpenAI Whisper not found")
        print("   Install with: pip3 install openai-whisper")
        return False
    
    # Check command-line tools
    if subprocess.run(['which', 'ffmpeg'], capture_output=True).returncode == 0:
        print("✓ FFmpeg found")
    else:
        print("❌ FFmpeg not found")
        print("   Install with: brew install ffmpeg")
        return False
    
    if subprocess.run(['which', 'sox'], capture_output=True).returncode == 0:
        print("✓ SoX found (recommended)")
    else:
        print("⚠️  SoX not found (optional but recommended)")
        print("   Install with: brew install sox")
    
    print("✓ Dependencies check complete")
    return True

def test_whisper_model(model_name="tiny"):
    """Test Whisper model download and basic functionality"""
    print(f"🤖 Testing Whisper {model_name} model...")
    
    try:
        import whisper
        
        # Load model (downloads if needed)
        print(f"Loading {model_name} model...")
        model = whisper.load_model(model_name)
        print(f"✓ {model_name.capitalize()} model loaded successfully")
        
        # Test with a simple audio file
        test_audio = create_test_audio()
        if test_audio:
            print("Testing transcription...")
            result = model.transcribe(test_audio)
            print(f"✓ Test transcription: '{result['text'].strip()}'")
            os.remove(test_audio)
        
        return True
        
    except Exception as e:
        print(f"❌ Whisper test failed: {e}")
        return False

def create_test_audio():
    """Create a test audio file using TTS"""
    print("🎵 Creating test audio...")
    
    try:
        with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as f:
            test_text = "Hello, this is a test of the speech recognition system."
            
            # Use macOS say command to create test audio
            cmd = [
                'say', '-v', 'Alex', '-o', f.name, '--data-format=LEI16@16000',
                test_text
            ]
            
            result = subprocess.run(cmd, capture_output=True, text=True)
            if result.returncode == 0:
                print(f"✓ Test audio created: {f.name}")
                return f.name
            else:
                print(f"❌ Failed to create test audio: {result.stderr}")
                return None
                
    except Exception as e:
        print(f"❌ Error creating test audio: {e}")
        return None

def test_microphone_recording():
    """Test microphone recording functionality"""
    print("🎤 Testing microphone recording...")
    
    try:
        with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as f:
            # Test recording with ffmpeg
            print("Recording 3 seconds of audio (speak now)...")
            
            cmd = [
                'ffmpeg', '-y', '-loglevel', 'quiet',
                '-f', 'avfoundation', '-i', ':0',
                '-t', '3', '-ar', '16000', '-ac', '1',
                '-sample_fmt', 's16', f.name
            ]
            
            result = subprocess.run(cmd)
            
            if result.returncode == 0 and os.path.exists(f.name):
                # Check file size
                file_size = os.path.getsize(f.name)
                if file_size > 1000:  # Should be at least 1KB for 3 seconds
                    print(f"✓ Recording successful ({file_size} bytes)")
                    
                    # Test playback
                    print("Playing back recorded audio...")
                    playback_cmd = ['afplay', f.name]
                    subprocess.run(playback_cmd)
                    
                    os.remove(f.name)
                    return True
                else:
                    print(f"❌ Recording file too small ({file_size} bytes)")
                    os.remove(f.name)
                    return False
            else:
                print("❌ Recording failed")
                return False
                
    except Exception as e:
        print(f"❌ Recording test failed: {e}")
        return False

def run_tts_test():
    """Test TTS functionality"""
    print("🗣️  Testing TTS functionality...")
    
    voices = ['Alex', 'Samantha', 'Victoria', 'Daniel']
    test_texts = [
        "Hello, I am the Hobot TTS system.",
        "I can speak with different voices.",
        "Speech synthesis is working correctly."
    ]
    
    for i, (voice, text) in enumerate(zip(voices, test_texts)):
        print(f"Testing voice {voice}: {text}")
        
        try:
            cmd = ['say', '-v', voice, text]
            result = subprocess.run(cmd, timeout=10)
            
            if result.returncode == 0:
                print(f"✓ Voice {voice} working")
            else:
                print(f"⚠️  Voice {voice} failed")
                
            time.sleep(1)  # Brief pause between tests
            
        except subprocess.TimeoutExpired:
            print(f"⚠️  Voice {voice} timed out")
        except Exception as e:
            print(f"❌ Voice {voice} error: {e}")
    
    print("✓ TTS test complete")

def run_stt_test():
    """Test STT functionality with live microphone"""
    print("🎤 Testing STT functionality...")
    
    if not test_whisper_model("tiny"):
        return False
    
    print("\nNow testing with live microphone...")
    print("When prompted, say: 'Hello robot, what time is it?'")
    input("Press Enter when ready to record...")
    
    try:
        import whisper
        model = whisper.load_model("tiny")
        
        # Record audio
        with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as f:
            print("🔴 Recording 5 seconds... speak now!")
            
            cmd = [
                'ffmpeg', '-y', '-loglevel', 'quiet',
                '-f', 'avfoundation', '-i', ':0',
                '-t', '5', '-ar', '16000', '-ac', '1',
                '-sample_fmt', 's16', f.name
            ]
            
            result = subprocess.run(cmd)
            
            if result.returncode == 0:
                print("🔇 Recording complete, processing...")
                
                # Transcribe
                result = model.transcribe(f.name)
                transcription = result['text'].strip()
                
                print(f"✓ Transcription: '{transcription}'")
                
                # Test TTS response
                response = f"You said: {transcription}"
                print(f"🗣️  TTS Response: {response}")
                subprocess.run(['say', '-v', 'Samantha', response])
                
                os.remove(f.name)
                return True
            else:
                print("❌ Recording failed")
                return False
                
    except Exception as e:
        print(f"❌ STT test failed: {e}")
        return False

def run_voice_assistant_demo():
    """Run a simple voice assistant demo"""
    print("🤖 Starting Voice Assistant Demo...")
    print("Wake word: 'Hello robot'")
    print("Commands: hello, time, weather, stop")
    print("Press Ctrl+C to exit")
    
    try:
        import whisper
        model = whisper.load_model("tiny")
        
        print("\n🎤 Voice assistant active - say 'Hello robot' to wake...")
        
        while True:
            try:
                # Record audio
                with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as f:
                    cmd = [
                        'ffmpeg', '-y', '-loglevel', 'quiet',
                        '-f', 'avfoundation', '-i', ':0',
                        '-t', '3', '-ar', '16000', '-ac', '1',
                        '-sample_fmt', 's16', f.name
                    ]
                    
                    result = subprocess.run(cmd)
                    
                    if result.returncode == 0 and os.path.getsize(f.name) > 1000:
                        # Transcribe
                        result = model.transcribe(f.name)
                        text = result['text'].strip().lower()
                        
                        if text and len(text) > 3:  # Ignore very short utterances
                            print(f"👂 Heard: '{text}'")
                            
                            # Check for wake word
                            if 'hello robot' in text:
                                print("🤖 Wake word detected!")
                                
                                # Extract command
                                if 'hello robot' in text:
                                    command = text.split('hello robot', 1)[1].strip()
                                    if command:
                                        handle_voice_command(command)
                                    else:
                                        speak("Yes, I'm listening. How can I help you?")
                    
                    os.remove(f.name)
                    
            except KeyboardInterrupt:
                print("\n👋 Voice assistant stopped")
                break
            except Exception as e:
                print(f"⚠️  Error: {e}")
                continue
                
    except Exception as e:
        print(f"❌ Voice assistant demo failed: {e}")

def handle_voice_command(command):
    """Handle voice assistant commands"""
    command = command.lower().strip()
    
    if 'hello' in command:
        speak("Hello! I'm the Hobot voice assistant.")
    elif 'time' in command:
        import datetime
        now = datetime.datetime.now()
        time_str = now.strftime("The current time is %I:%M %p")
        speak(time_str)
    elif 'weather' in command:
        speak("I don't have access to weather information yet, but it's always a good day for robotics!")
    elif 'stop' in command or 'quit' in command:
        speak("Goodbye!")
        exit(0)
    else:
        speak(f"I heard you say {command}. I'm still learning how to respond to different commands.")

def speak(text):
    """Speak text using TTS"""
    print(f"🗣️  Speaking: {text}")
    subprocess.run(['say', '-v', 'Samantha', text])

def main():
    print("=" * 60)
    print("🤖 Hobot TTS+STT System Demo")
    print("=" * 60)
    
    # Check dependencies
    if not check_dependencies():
        print("\n❌ Dependencies missing. Please install required packages.")
        return 1
    
    print("\nChoose a demo:")
    print("1. Test TTS (Text-to-Speech)")
    print("2. Test STT (Speech-to-Text)")
    print("3. Test Microphone Recording")
    print("4. Voice Assistant Demo")
    print("5. Run All Tests")
    print("0. Exit")
    
    while True:
        try:
            choice = input("\nEnter choice (0-5): ").strip()
            
            if choice == '0':
                print("👋 Goodbye!")
                break
            elif choice == '1':
                run_tts_test()
            elif choice == '2':
                run_stt_test()
            elif choice == '3':
                test_microphone_recording()
            elif choice == '4':
                run_voice_assistant_demo()
            elif choice == '5':
                print("\n🧪 Running all tests...")
                run_tts_test()
                print()
                test_microphone_recording()
                print()
                run_stt_test()
                print("\n✓ All tests complete!")
            else:
                print("Invalid choice. Please enter 0-5.")
                
        except KeyboardInterrupt:
            print("\n\n👋 Demo interrupted. Goodbye!")
            break
        except Exception as e:
            print(f"\n❌ Error: {e}")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
