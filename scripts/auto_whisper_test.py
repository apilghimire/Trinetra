#!/usr/bin/env python3
"""
Automatic Whisper STT test - records immediately without user input
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

def test_whisper_auto():
    """Test Whisper STT with automatic recording"""
    print("🤖 Automatic Whisper STT Test")
    print("=" * 40)
    
    # Load model
    print("Loading Whisper tiny model...")
    try:
        model = whisper.load_model("tiny")
        print("✓ Model loaded successfully")
    except Exception as e:
        print(f"❌ Failed to load model: {e}")
        return False
    
    # Give user time to prepare
    print("\n🎬 Starting automatic recording in:")
    for i in range(3, 0, -1):
        print(f"   {i}...")
        time.sleep(1)
    
    # Record audio automatically
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
        
        print(f"\n✅ RESULTS:")
        print(f"✅ Transcription: '{transcription}'")
        print(f"✅ Language: {language}")
        print(f"✅ Processing time: {processing_time:.0f}ms")
        
        # Speak back the result
        if transcription:
            response = f"You said: {transcription}"
            print(f"\n🗣️  Speaking back: {response}")
            subprocess.run(['say', '-v', 'Samantha', response])
        
        return True
        
    except Exception as e:
        print(f"❌ Transcription failed: {e}")
        return False
    finally:
        # Clean up
        if os.path.exists(audio_file):
            os.remove(audio_file)

def quick_demo():
    """Quick demo with predefined phrases"""
    print("🎤 Quick Whisper Demo")
    print("=" * 30)
    
    print("This will test common phrases...")
    phrases = [
        "Hello, this is a test of speech recognition.",
        "The weather is nice today.",
        "What time is it?"
    ]
    
    # Load model
    try:
        model = whisper.load_model("tiny")
        print("✓ Whisper model ready")
    except Exception as e:
        print(f"❌ Model loading failed: {e}")
        return
    
    for i, phrase in enumerate(phrases, 1):
        print(f"\n🎯 Test {i}/3: Say '{phrase}'")
        print("Recording in:")
        for j in range(3, 0, -1):
            print(f"   {j}...")
            time.sleep(1)
        
        audio_file = record_audio(4)
        if audio_file:
            try:
                result = model.transcribe(audio_file)
                transcription = result["text"].strip()
                
                print(f"Expected: '{phrase}'")
                print(f"Got:      '{transcription}'")
                
                # Simple accuracy check
                if phrase.lower() in transcription.lower() or transcription.lower() in phrase.lower():
                    print("✅ Match!")
                else:
                    print("❌ Different")
                
            except Exception as e:
                print(f"❌ Error: {e}")
            finally:
                os.remove(audio_file)
        else:
            print("❌ Recording failed")
    
    print("\n🎉 Demo complete!")

def main():
    print("🎤 Whisper STT Quick Test")
    print("=" * 30)
    
    # Check Whisper
    try:
        import whisper
        print("✓ Whisper available")
    except ImportError:
        print("❌ Whisper not found - install with: pip3 install openai-whisper")
        return 1
    
    print("\nChoose test mode:")
    print("1. Auto Test (record 5 seconds automatically)")
    print("2. Quick Demo (test 3 predefined phrases)")
    print("0. Exit")
    
    choice = input("\nChoice: ").strip()
    
    if choice == '1':
        test_whisper_auto()
    elif choice == '2':
        quick_demo()
    elif choice == '0':
        print("👋 Goodbye!")
    else:
        print("Invalid choice")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
