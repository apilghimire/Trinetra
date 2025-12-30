# Quick Start Guide for Hobot TTS macOS

## What is this?

This is a macOS-compatible version of the D-Robotics Hobot TTS (Text-to-Speech) system. The original system was designed for RDK (Robot Development Kit) hardware running Linux, but this version adapts the functionality to work on macOS using built-in tools.

## Key Features

✅ **Text-to-Speech**: Convert text messages to speech  
✅ **Multiple TTS Engines**: Support for macOS `say`, Festival, and eSpeak  
✅ **ROS2 Integration**: Full ROS2 node implementation  
✅ **Audio Playback**: Native macOS audio support  
✅ **Chinese/English Support**: Handles multiple languages  
✅ **Queue Management**: Message and audio processing queues  

## Quick Test (No ROS2 Required)

1. **Test basic TTS functionality**:
   ```bash
   cd /Users/apilghimire/Documents/VLM_Test
   python3 scripts/demo.py
   ```

2. **Test with different voices**:
   ```bash
   say -v Alex "Hello, I'm Alex"
   say -v Samantha "Hello, I'm Samantha"
   say -v Fiona "Hello, I'm Fiona"  # Scottish accent
   ```

3. **See all available voices**:
   ```bash
   say -v \? | head -10
   ```

## Building the Project

### Option 1: Standalone Build (Recommended for testing)
```bash
cd /Users/apilghimire/Documents/VLM_Test
./scripts/build_standalone.sh
./standalone_build/hobot_tts_test
```

### Option 2: ROS2 Build (For full functionality)
```bash
# Install ROS2 if not installed
brew install ros

# Build the project
cd /Users/apilghimire/Documents/VLM_Test
colcon build --packages-select hobot_tts_macos
source install/setup.bash
```

## Usage Examples

### Using the ROS2 Node

1. **Start the TTS node**:
   ```bash
   source install/setup.bash
   ros2 run hobot_tts_macos hobot_tts_node
   ```

2. **Send text messages** (in another terminal):
   ```bash
   source install/setup.bash
   
   # Basic message
   ros2 topic pub --once /tts_text std_msgs/msg/String '{data: "Hello, world!"}'
   
   # Longer message
   ros2 topic pub --once /tts_text std_msgs/msg/String '{data: "This is a test of the Hobot TTS system running on macOS."}'
   
   # Chinese text (if you have Chinese voices)
   ros2 topic pub --once /tts_text std_msgs/msg/String '{data: "你好，世界！"}'
   ```

3. **Use different voices**:
   ```bash
   # Start with specific voice
   ros2 run hobot_tts_macos hobot_tts_node --ros-args -p voice_name:="Alex"
   
   # Or use different engines
   ros2 run hobot_tts_macos hobot_tts_node --ros-args -p tts_engine:="festival"
   ```

### Using Launch Files

1. **Basic launch**:
   ```bash
   ros2 launch hobot_tts_macos hobot_tts.launch.py
   ```

2. **Launch with custom settings**:
   ```bash
   ros2 launch hobot_tts_macos hobot_tts.launch.py \
     voice_name:="Samantha" \
     tts_engine:="say" \
     topic_sub:="/my_tts_text"
   ```

3. **Demo launch** (includes test messages):
   ```bash
   ros2 launch hobot_tts_macos demo.launch.py
   ```

## Testing Different Scenarios

### 1. English Text
```bash
ros2 topic pub --once /tts_text std_msgs/msg/String '{data: "The quick brown fox jumps over the lazy dog. This tests various English phonemes."}'
```

### 2. Numbers and Punctuation
```bash
ros2 topic pub --once /tts_text std_msgs/msg/String '{data: "Testing numbers: 1, 2, 3. And punctuation! Are you listening? Yes, I am."}'
```

### 3. Technical Terms
```bash
ros2 topic pub --once /tts_text std_msgs/msg/String '{data: "ROS2 node initialized. Processing text-to-speech synthesis using macOS CoreAudio framework."}'
```

### 4. Long Text (tests segmentation)
```bash
ros2 topic pub --once /tts_text std_msgs/msg/String '{data: "This is a longer text to test the text segmentation functionality. It should be broken down into smaller segments. Each segment will be processed separately for better speech synthesis quality."}'
```

## Available Parameters

| Parameter | Description | Default | Options |
|-----------|-------------|---------|---------|
| `topic_sub` | Input topic for text | `/tts_text` | Any valid topic name |
| `tts_engine` | TTS engine to use | `say` | `say`, `festival`, `espeak` |
| `voice_name` | Voice for synthesis | `default` | See voice lists below |
| `audio_device` | Audio output device | `default` | `default`, `coreaudio` |

## Voice Options

### macOS Say Voices (Examples)
- **Alex** - Default male voice
- **Samantha** - Female voice  
- **Fiona** - Scottish accent
- **Victoria** - British accent
- **Fred** - Older male voice
- **Kathy** - Robotic voice
- **Moira** - Irish accent

Get full list: `say -v \?`

## Troubleshooting

### Common Issues

**1. No audio output**
```bash
# Test system audio
say "test audio"

# Check audio devices
system_profiler SPAudioDataType
```

**2. TTS engine not found**
```bash
# Check available engines
which say        # Should show /usr/bin/say
which festival   # Optional
which espeak     # Optional
```

**3. ROS2 issues**
```bash
# Check ROS2 installation
ros2 --version

# Source setup (if using colcon build)
source install/setup.bash

# Check if node is running
ros2 node list
```

**4. Permission issues**
```bash
# Make scripts executable
chmod +x scripts/*.sh scripts/*.py
```

### Performance Tips

**1. Reduce audio latency**
```bash
ros2 run hobot_tts_macos hobot_tts_node --ros-args -p audio_device:="coreaudio"
```

**2. Use faster TTS engine**
```bash
ros2 run hobot_tts_macos hobot_tts_node --ros-args -p tts_engine:="say"
```

**3. Optimize for real-time**
```bash
# Reduce queue sizes in code if needed
# Current settings: max 10 messages, max 5 audio buffers
```

## Integration Examples

### 1. Chat Bot Integration
```python
import rclpy
from std_msgs.msg import String

# In your chat bot node:
tts_pub = node.create_publisher(String, '/tts_text', 10)
tts_pub.publish(String(data="Hello, how can I help you?"))
```

### 2. Robot Navigation Feedback
```bash
# When robot reaches destination
ros2 topic pub --once /tts_text std_msgs/msg/String '{data: "Destination reached. Mission complete."}'

# When obstacle detected
ros2 topic pub --once /tts_text std_msgs/msg/String '{data: "Obstacle detected. Rerouting path."}'
```

### 3. System Status Announcements
```bash
# System startup
ros2 topic pub --once /tts_text std_msgs/msg/String '{data: "System initialized and ready for operation."}'

# Low battery warning
ros2 topic pub --once /tts_text std_msgs/msg/String '{data: "Warning: Battery level is low. Please connect charger."}'
```

## Next Steps

1. **Install additional TTS engines** for more voice options:
   ```bash
   brew install festival espeak
   ```

2. **Customize voices** by editing the configuration:
   ```bash
   nano config/tts_config.yaml
   ```

3. **Integrate with your application** by publishing to `/tts_text` topic

4. **Explore the original documentation**: https://d-robotics.github.io/rdk_doc/en/Robot_development/quick_demo/hobot_tts/

## Support

- Test your setup: `./scripts/test.sh`
- Run demos: `python3 scripts/demo.py`
- Check logs: `ros2 run hobot_tts_macos hobot_tts_node --ros-args --log-level debug`

Enjoy using the Hobot TTS system on macOS! 🎉
