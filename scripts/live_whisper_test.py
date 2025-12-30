#!/usr/bin/env python3
"""
Live microphone test for Whisper STT
"""

import whisper
import tempfile
import subprocess
import sys
import time
import os

def main():
    print("🎤 Live Whisper STT Test")
    print("=" * 30)
    
    print("🔊 Please speak clearly when recording starts")
    print("⏱️  Get ready... recording will start in:")
    for i in range(3, 0, -1):
        print(f"   {i}...")
        time.sleep(1)

    with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as f:
        print('🔴 RECORDING NOW (4 seconds)')
        print('📢 Say: "This is a microphone test for speech recognition"')
        
        cmd = ['ffmpeg', '-y', '-loglevel', 'warning', '-f', 'avfoundation', '-i', ':0', 
               '-t', '4', '-ar', '16000', '-ac', '1', '-sample_fmt', 's16', f.name]
        
        subprocess.run(cmd)
        
        if os.path.getsize(f.name) > 1000:
            print(f'✓ Recording complete: {os.path.getsize(f.name)} bytes')
            
            # Test playback
            print('🔊 Playing back recording...')
            subprocess.run(['afplay', f.name])
            
            # Test with Whisper
            print('🤖 Processing with Whisper...')
            model = whisper.load_model('tiny')
            result = model.transcribe(f.name)
            
            transcription = result['text'].strip()
            if transcription:
                print(f'✅ Whisper result: "{transcription}"')
                print(f'🗣️  Speaking back...')
                subprocess.run(['say', f'You said: {transcription}'])
            else:
                print('❌ No speech detected')
        else:
            print('❌ Recording failed or too small')
        
        os.remove(f.name)

if __name__ == "__main__":
    main()
