#!/usr/bin/env python3
"""
OpenAI Whisper API Demo Script
Simple Python script to test speech-to-text with OpenAI's Whisper API
"""

import os
import sys
import time
import tempfile
import subprocess
import json
from pathlib import Path

def check_dependencies():
    """Check if required dependencies are available"""
    print("🔍 Checking dependencies...")
    
    # Check Python packages
    try:
        import requests
        print("✓ requests library found")
    except ImportError:
        print("❌ requests not found")
        print("   Install with: pip install requests")
        return False
    
    # Check command-line tools
    if subprocess.run(['which', 'ffmpeg'], capture_output=True).returncode == 0:
        print("✓ FFmpeg found")
    else:
        print("❌ FFmpeg not found")
        print("   Install with: brew install ffmpeg")
        return False
    
    print("✓ Dependencies check complete")
    return True

def get_api_key():
    """Get OpenAI API key from environment or user input"""
    # Try environment variable first
    api_key = os.getenv('OPENAI_API_KEY')
    if api_key:
        print("✓ Using API key from OPENAI_API_KEY environment variable")
        return api_key
    
    # Ask user for API key
    print("\n🔑 OpenAI API key not found in environment variable OPENAI_API_KEY")
    api_key = input("Please enter your OpenAI API key: ").strip()
    
    if not api_key:
        print("❌ No API key provided")
        return None
    
    return api_key

def record_audio(duration=5):
    """Record audio using ffmpeg"""
    print(f"🎤 Recording {duration} seconds of audio... speak now!")
    
    with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as temp_file:
        cmd = [
            'ffmpeg', '-y', '-loglevel', 'quiet',
            '-f', 'avfoundation', '-i', ':0',
            '-t', str(duration), '-ar', '16000', '-ac', '1',
            '-sample_fmt', 's16', temp_file.name
        ]
        
        try:
            subprocess.run(cmd, check=True)
            if os.path.getsize(temp_file.name) > 1000:  # At least 1KB
                print(f"✓ Recording complete: {temp_file.name}")
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

def convert_to_mp3(wav_file):
    """Convert WAV to MP3 for API upload"""
    mp3_file = wav_file.replace('.wav', '.mp3')
    
    cmd = [
        'ffmpeg', '-y', '-loglevel', 'quiet',
        '-i', wav_file, '-acodec', 'libmp3lame',
        '-b:a', '64k', mp3_file
    ]
    
    try:
        subprocess.run(cmd, check=True)
        os.remove(wav_file)  # Remove original WAV
        return mp3_file
    except subprocess.CalledProcessError:
        print("❌ Failed to convert to MP3")
        return None

def transcribe_with_openai(audio_file, api_key, model="whisper-1", language=None):
    """Send audio to OpenAI Whisper API for transcription"""
    import requests
    
    url = "https://api.openai.com/v1/audio/transcriptions"
    headers = {
        "Authorization": f"Bearer {api_key}"
    }
    
    files = {
        "file": open(audio_file, "rb")
    }
    
    data = {
        "model": model,
        "response_format": "json"
    }
    
    if language:
        data["language"] = language
    
    try:
        print("🤖 Sending audio to OpenAI Whisper API...")
        start_time = time.time()
        
        response = requests.post(url, headers=headers, files=files, data=data)
        
        end_time = time.time()
        processing_time = (end_time - start_time) * 1000
        
        files["file"].close()
        
        if response.status_code == 200:
            result = response.json()
            return {
                "text": result.get("text", "").strip(),
                "success": True,
                "processing_time_ms": processing_time
            }
        else:
            print(f"❌ API Error {response.status_code}: {response.text}")
            return {
                "text": "",
                "success": False,
                "processing_time_ms": processing_time
            }
            
    except Exception as e:
        print(f"❌ Request failed: {e}")
        return {
            "text": "",
            "success": False,
            "processing_time_ms": 0
        }

def speak_text(text):
    """Use macOS say command to speak text"""
    cmd = ['say', '-v', 'Samantha', text]
    subprocess.run(cmd)

def test_api_stt():
    """Test basic STT functionality with OpenAI API"""
    print("\n🎤 Testing OpenAI Whisper API STT...")
    
    api_key = get_api_key()
    if not api_key:
        return
    
    print("\nWhen ready, press Enter and speak a test phrase...")
    input()
    
    # Record audio
    audio_file = record_audio(5)
    if not audio_file:
        return
    
    # Convert to MP3 (API prefers MP3)
    mp3_file = convert_to_mp3(audio_file)
    if not mp3_file:
        return
    
    try:
        # Transcribe with OpenAI API
        result = transcribe_with_openai(mp3_file, api_key)
        
        if result["success"]:
            print(f"✅ Transcription: \"{result['text']}\"")
            print(f"✅ Processing time: {result['processing_time_ms']:.0f}ms")
            
            # Speak back the result
            response = f"You said: {result['text']}"
            print(f"🗣️  Speaking: {response}")
            speak_text(response)
        else:
            print("❌ Transcription failed")
    
    finally:
        # Clean up
        if os.path.exists(mp3_file):
            os.remove(mp3_file)

def voice_assistant_demo():
    """Interactive voice assistant using OpenAI API"""
    print("\n🤖 Voice Assistant Demo with OpenAI Whisper API")
    print("Wake phrase: 'Hey assistant'")
    print("Commands: hello, time, weather, stop")
    print("Press Ctrl+C to exit\n")
    
    api_key = get_api_key()
    if not api_key:
        return
    
    speak_text("Voice assistant ready. Say hey assistant to wake me up.")
    
    try:
        while True:
            print("🎤 Listening... (3 seconds)")
            
            # Record audio
            audio_file = record_audio(3)
            if not audio_file:
                continue
            
            # Convert to MP3
            mp3_file = convert_to_mp3(audio_file)
            if not mp3_file:
                continue
            
            try:
                # Transcribe
                result = transcribe_with_openai(mp3_file, api_key)
                
                if result["success"] and result["text"]:
                    text = result["text"].lower()
                    print(f"👂 Heard: \"{result['text']}\"")
                    
                    # Check for wake phrase
                    if "hey assistant" in text:
                        print("🤖 Wake phrase detected!")
                        
                        # Extract command after wake phrase
                        if "hey assistant" in text:
                            command = text.split("hey assistant", 1)[1].strip()
                            if command:
                                handle_voice_command(command)
                            else:
                                speak_text("Yes, I'm listening. How can I help you?")
                
            finally:
                # Clean up
                if os.path.exists(mp3_file):
                    os.remove(mp3_file)
                    
    except KeyboardInterrupt:
        print("\n👋 Voice assistant stopped")

def handle_voice_command(command):
    """Handle voice assistant commands"""
    command = command.lower().strip()
    print(f"🤖 Processing command: {command}")
    
    if "hello" in command:
        speak_text("Hello! I'm using the OpenAI Whisper API for speech recognition.")
    elif "time" in command:
        import datetime
        now = datetime.datetime.now()
        time_str = now.strftime("The current time is %I:%M %p")
        speak_text(time_str)
    elif "weather" in command:
        speak_text("I don't have access to weather information yet, but the OpenAI API is working great!")
    elif "stop" in command or "quit" in command:
        speak_text("Goodbye!")
        sys.exit(0)
    else:
        speak_text(f"I heard you say {command}. I'm using OpenAI Whisper API for recognition.")

def test_different_languages():
    """Test STT with different languages"""
    print("\n🌍 Testing different languages...")
    
    api_key = get_api_key()
    if not api_key:
        return
    
    languages = {
        "en": "English",
        "es": "Spanish", 
        "fr": "French",
        "de": "German",
        "it": "Italian",
        "pt": "Portuguese",
        "ja": "Japanese",
        "ko": "Korean",
        "zh": "Chinese"
    }
    
    print("\nAvailable languages:")
    for code, name in languages.items():
        print(f"  {code}: {name}")
    
    lang_code = input("\nEnter language code (or press Enter for auto-detect): ").strip()
    if lang_code and lang_code not in languages:
        print("❌ Invalid language code")
        return
    
    language = lang_code if lang_code else None
    lang_name = languages.get(lang_code, "auto-detect") if lang_code else "auto-detect"
    
    print(f"\nLanguage set to: {lang_name}")
    print("When ready, press Enter and speak in the selected language...")
    input()
    
    # Record audio
    audio_file = record_audio(5)
    if not audio_file:
        return
    
    # Convert to MP3
    mp3_file = convert_to_mp3(audio_file)
    if not mp3_file:
        return
    
    try:
        # Transcribe with language setting
        result = transcribe_with_openai(mp3_file, api_key, language=language)
        
        if result["success"]:
            print(f"✅ Transcription ({lang_name}): \"{result['text']}\"")
            print(f"✅ Processing time: {result['processing_time_ms']:.0f}ms")
        else:
            print("❌ Transcription failed")
    
    finally:
        # Clean up
        if os.path.exists(mp3_file):
            os.remove(mp3_file)

def main():
    print("=" * 60)
    print("🤖 OpenAI Whisper API Demo")
    print("=" * 60)
    
    # Check dependencies
    if not check_dependencies():
        print("\n❌ Dependencies missing. Please install required packages.")
        return 1
    
    print("\n💡 Get your API key at: https://platform.openai.com/api-keys")
    print("💡 Set environment variable: export OPENAI_API_KEY=your_key_here")
    
    print("\nChoose a demo:")
    print("1. Test OpenAI Whisper API STT")
    print("2. Voice Assistant Demo")
    print("3. Test Different Languages")
    print("0. Exit")
    
    while True:
        try:
            choice = input("\nEnter choice (0-3): ").strip()
            
            if choice == '0':
                print("👋 Goodbye!")
                break
            elif choice == '1':
                test_api_stt()
            elif choice == '2':
                voice_assistant_demo()
            elif choice == '3':
                test_different_languages()
            else:
                print("Invalid choice. Please enter 0-3.")
                
        except KeyboardInterrupt:
            print("\n\n👋 Demo interrupted. Goodbye!")
            break
        except Exception as e:
            print(f"\n❌ Error: {e}")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
