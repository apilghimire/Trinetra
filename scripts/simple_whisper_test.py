#!/usr/bin/env python3
"""
Simple Whisper STT test using local models
"""

import whisper
import tempfile
import subprocess
import sys
import time
import os

def record_audio(duration=5):
    """Record audio for the specified duration"""
    print(f"🎤 Recording {duration} seconds... speak now!")
    
    with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as temp_file:
        cmd = [
            'ffmpeg', '-y', '-loglevel', 'quiet',
            '-f', 'avfoundation', '-i', ':0',
            '-t', str(duration), '-ar', '16000', '-ac', '1',
            '-sample_fmt', 's16', temp_file.name
        ]
        
        try:
            subprocess.run(cmd, check=True)
            if os.path.getsize(temp_file.name) > 1000:
                print(f"✓ Recording saved: {os.path.getsize(temp_file.name)} bytes")
                return temp_file.name
            else:
                print("❌ Recording file too small")
                os.remove(temp_file.name)
                return None
        except subprocess.CalledProcessError:
            print("❌ Recording failed")
            if os.path.exists(temp_file.name):
                os.remove(temp_file.name)
            return None

def test_whisper_stt():
    """Test Whisper STT functionality"""
    print("🤖 Testing Local Whisper STT...")
    
    # Load model
    print("Loading Whisper tiny model...")
    try:
        model = whisper.load_model("tiny")
        print("✓ Model loaded successfully")
    except Exception as e:
        print(f"❌ Failed to load model: {e}")
        return False
    
    # Record audio
    print("\nWhen ready, press Enter to start recording...")
    input()
    
    audio_file = record_audio(5)
    if not audio_file:
        return False
    
    try:
        # Transcribe
        print("🧠 Processing with Whisper...")
        start_time = time.time()
        
        result = model.transcribe(audio_file)
        
        end_time = time.time()
        processing_time = (end_time - start_time) * 1000
        
        # Results
        transcription = result["text"].strip()
        language = result.get("language", "unknown")
        
        print(f"✅ Transcription: '{transcription}'")
        print(f"✅ Language: {language}")
        print(f"✅ Processing time: {processing_time:.0f}ms")
        
        # Speak back the result
        if transcription:
            response = f"You said: {transcription}"
            print(f"🗣️  Speaking: {response}")
            subprocess.run(['say', '-v', 'Samantha', response])
        
        return True
        
    except Exception as e:
        print(f"❌ Transcription failed: {e}")
        return False
    finally:
        # Clean up
        if os.path.exists(audio_file):
            os.remove(audio_file)

def voice_assistant_demo():
    """Simple voice assistant demo"""
    print("🤖 Voice Assistant Demo with Local Whisper")
    print("Wake phrase: 'Hey computer'")
    print("Commands: hello, time, weather, stop")
    print("Press Ctrl+C to exit\n")
    
    # Load model
    try:
        model = whisper.load_model("tiny")
        print("✓ Whisper model loaded")
    except Exception as e:
        print(f"❌ Failed to load model: {e}")
        return
    
    subprocess.run(['say', 'Voice assistant ready. Say hey computer to wake me up.'])
    
    try:
        while True:
            print("🎤 Listening... (3 seconds)")
            
            audio_file = record_audio(3)
            if not audio_file:
                continue
            
            try:
                # Quick transcription
                result = model.transcribe(audio_file)
                text = result["text"].strip().lower()
                
                if text and len(text) > 3:
                    print(f"👂 Heard: '{text}'")
                    
                    # Check for wake phrase
                    if "hey computer" in text:
                        print("🤖 Wake phrase detected!")
                        
                        # Extract command
                        if "hey computer" in text:
                            command = text.split("hey computer", 1)[1].strip()
                            if command:
                                handle_command(command)
                            else:
                                subprocess.run(['say', "Yes, I'm listening. How can I help you?"])
                
            except Exception as e:
                print(f"⚠️  Processing error: {e}")
            finally:
                if os.path.exists(audio_file):
                    os.remove(audio_file)
                    
    except KeyboardInterrupt:
        print("\n👋 Voice assistant stopped")

def handle_command(command):
    """Handle voice assistant commands"""
    print(f"🤖 Processing command: '{command}'")
    
    if "hello" in command:
        subprocess.run(['say', "Hello! I'm using local Whisper for speech recognition."])
    elif "time" in command:
        import datetime
        now = datetime.datetime.now()
        time_str = now.strftime("The current time is %I:%M %p")
        subprocess.run(['say', time_str])
    elif "weather" in command:
        subprocess.run(['say', "I don't have access to weather information yet, but local Whisper is working great!"])
    elif "stop" in command or "quit" in command:
        subprocess.run(['say', "Goodbye!"])
        sys.exit(0)
    else:
        subprocess.run(['say', f"I heard you say {command}. I'm using local Whisper for recognition."])

def check_dependencies():
    """Check if required dependencies are available"""
    print("🔍 Checking dependencies...")
    
    # Check Whisper
    try:
        import whisper
        print("✓ OpenAI Whisper library found")
    except ImportError:
        print("❌ OpenAI Whisper not found")
        print("   Install with: pip3 install openai-whisper")
        return False
    
    # Check ffmpeg
    if subprocess.run(['which', 'ffmpeg'], capture_output=True).returncode == 0:
        print("✓ FFmpeg found")
    else:
        print("❌ FFmpeg not found")
        print("   Install with: brew install ffmpeg")
        return False
    
    print("✓ All dependencies available")
    return True

def main():
    print("=" * 50)
    print("🎤 Local Whisper STT Test")
    print("=" * 50)
    
    # Check dependencies
    if not check_dependencies():
        print("\n❌ Please install missing dependencies")
        return 1
    
    print("\nChoose a test:")
    print("1. Test STT (Speech-to-Text)")
    print("2. Voice Assistant Demo")
    print("0. Exit")
    
    try:
        choice = input("\nEnter choice (0-2): ").strip()
        
        if choice == '0':
            print("👋 Goodbye!")
        elif choice == '1':
            test_whisper_stt()
        elif choice == '2':
            voice_assistant_demo()
        else:
            print("Invalid choice")
            
    except KeyboardInterrupt:
        print("\n\n👋 Demo interrupted. Goodbye!")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
