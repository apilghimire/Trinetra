#!/usr/bin/env python3

"""
Simple demo script to test the Hobot TTS system without ROS2
This can be used to test TTS engines directly
"""

import subprocess
import tempfile
import os
import sys

def test_say_engine(text, voice="default"):
    """Test macOS say command"""
    try:
        cmd = ["say"]
        if voice != "default":
            cmd.extend(["-v", voice])
        cmd.append(text)
        
        print(f"Testing say engine with voice '{voice}': {text}")
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        
        if result.returncode == 0:
            print("✓ Say engine test successful")
            return True
        else:
            print(f"✗ Say engine failed: {result.stderr}")
            return False
            
    except subprocess.TimeoutExpired:
        print("✗ Say engine timed out")
        return False
    except Exception as e:
        print(f"✗ Say engine error: {e}")
        return False

def test_festival_engine(text):
    """Test Festival TTS"""
    try:
        print(f"Testing Festival engine: {text}")
        
        # Create temporary text file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
            f.write(text)
            text_file = f.name
        
        # Create temporary audio file
        audio_file = text_file.replace('.txt', '.wav')
        
        cmd = ["festival", "--tts", text_file, "--otype", "wav", "--output", audio_file]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        
        # Clean up
        os.unlink(text_file)
        
        if result.returncode == 0 and os.path.exists(audio_file):
            # Play the audio
            if os.path.exists("/usr/bin/afplay"):
                subprocess.run(["afplay", audio_file])
            elif os.path.exists("/usr/bin/aplay"):
                subprocess.run(["aplay", audio_file])
            
            os.unlink(audio_file)
            print("✓ Festival engine test successful")
            return True
        else:
            if os.path.exists(audio_file):
                os.unlink(audio_file)
            print(f"✗ Festival engine failed: {result.stderr}")
            return False
            
    except subprocess.TimeoutExpired:
        print("✗ Festival engine timed out")
        return False
    except Exception as e:
        print(f"✗ Festival engine error: {e}")
        return False

def test_espeak_engine(text, voice="en"):
    """Test eSpeak TTS"""
    try:
        print(f"Testing eSpeak engine with voice '{voice}': {text}")
        
        # Create temporary audio file
        with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as f:
            audio_file = f.name
        
        cmd = ["espeak", "-v", voice, "-w", audio_file, text]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        
        if result.returncode == 0 and os.path.exists(audio_file):
            # Play the audio
            if os.path.exists("/usr/bin/afplay"):
                subprocess.run(["afplay", audio_file])
            elif os.path.exists("/usr/bin/aplay"):
                subprocess.run(["aplay", audio_file])
            
            os.unlink(audio_file)
            print("✓ eSpeak engine test successful")
            return True
        else:
            if os.path.exists(audio_file):
                os.unlink(audio_file)
            print(f"✗ eSpeak engine failed: {result.stderr}")
            return False
            
    except subprocess.TimeoutExpired:
        print("✗ eSpeak engine timed out")
        return False
    except Exception as e:
        print(f"✗ eSpeak engine error: {e}")
        return False

def get_available_voices():
    """Get available voices for different engines"""
    print("\nAvailable voices:")
    
    # Say voices
    try:
        result = subprocess.run(["say", "-v", "?"], capture_output=True, text=True)
        if result.returncode == 0:
            lines = result.stdout.strip().split('\n')[:5]  # Show first 5
            print("  Say voices (first 5):")
            for line in lines:
                voice_name = line.split()[0]
                print(f"    - {voice_name}")
            print("    ...")
    except:
        pass
    
    # eSpeak voices
    try:
        result = subprocess.run(["espeak", "--voices"], capture_output=True, text=True)
        if result.returncode == 0:
            lines = result.stdout.strip().split('\n')[1:6]  # Skip header, show first 5
            print("  eSpeak voices (first 5):")
            for line in lines:
                if line.strip():
                    parts = line.split()
                    if len(parts) >= 2:
                        print(f"    - {parts[1]}")
            print("    ...")
    except:
        pass

def main():
    print("=== Hobot TTS Demo ===")
    print()
    
    test_texts = [
        "Hello, this is a test of the text to speech system.",
        "The quick brown fox jumps over the lazy dog.",
        "Testing numbers: one, two, three, four, five.",
        "Welcome to the Hobot TTS demonstration."
    ]
    
    # Check available engines
    available_engines = []
    
    if subprocess.run(["which", "say"], capture_output=True).returncode == 0:
        available_engines.append("say")
    
    if subprocess.run(["which", "festival"], capture_output=True).returncode == 0:
        available_engines.append("festival")
    
    if subprocess.run(["which", "espeak"], capture_output=True).returncode == 0:
        available_engines.append("espeak")
    
    if not available_engines:
        print("No TTS engines found!")
        return
    
    print(f"Available engines: {', '.join(available_engines)}")
    
    # Show available voices
    get_available_voices()
    
    # Test each engine with different texts
    for i, text in enumerate(test_texts):
        print(f"\n--- Test {i+1}: {text} ---")
        
        if "say" in available_engines:
            test_say_engine(text)
            print()
        
        if "festival" in available_engines:
            test_festival_engine(text)
            print()
        
        if "espeak" in available_engines:
            test_espeak_engine(text)
            print()
        
        # Wait a bit between tests
        input("Press Enter to continue to next test...")
    
    print("\n=== Demo completed ===")
    print("\nTo test with ROS2:")
    print("1. Build the project: colcon build --packages-select hobot_tts_macos")
    print("2. Source setup: source install/setup.bash")
    print("3. Run node: ros2 run hobot_tts_macos hobot_tts_node")
    print("4. Send message: ros2 topic pub --once /tts_text std_msgs/msg/String '{data: \"Hello ROS2!\"}'")

if __name__ == "__main__":
    main()
