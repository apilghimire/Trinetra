#!/usr/bin/env python3

from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument, ExecuteProcess
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    return LaunchDescription([
        # TTS Node with default settings
        Node(
            package='hobot_tts_macos',
            executable='hobot_tts_node',
            name='hobot_tts_node',
            parameters=[{
                'topic_sub': '/tts_text',
                'audio_device': 'default',
                'tts_engine': 'say',
                'voice_name': 'default',
            }],
            output='screen'
        ),
        
        # Test publisher - sends test messages every 5 seconds
        ExecuteProcess(
            cmd=[
                'ros2', 'topic', 'pub', '--rate', '0.2',
                '/tts_text', 'std_msgs/msg/String',
                '"{data: \"Hello, this is a test of the text to speech system. The current time is $(date)\"}"'
            ],
            output='screen',
            shell=True
        ),
    ])
