#!/usr/bin/env python3

from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    return LaunchDescription([
        # Declare launch arguments
        DeclareLaunchArgument(
            'topic_sub',
            default_value='/tts_text',
            description='Topic to subscribe for text messages'
        ),
        
        DeclareLaunchArgument(
            'audio_device',
            default_value='default',
            description='Audio output device'
        ),
        
        DeclareLaunchArgument(
            'tts_engine',
            default_value='say',
            description='TTS engine to use (say, festival, espeak)'
        ),
        
        DeclareLaunchArgument(
            'voice_name',
            default_value='default',
            description='Voice name for TTS'
        ),
        
        DeclareLaunchArgument(
            'log_level',
            default_value='info',
            description='Log level (debug, info, warn, error)'
        ),
        
        # TTS Node
        Node(
            package='hobot_tts_macos',
            executable='hobot_tts_node',
            name='hobot_tts_node',
            parameters=[{
                'topic_sub': LaunchConfiguration('topic_sub'),
                'audio_device': LaunchConfiguration('audio_device'),
                'tts_engine': LaunchConfiguration('tts_engine'),
                'voice_name': LaunchConfiguration('voice_name'),
            }],
            arguments=['--ros-args', '--log-level', LaunchConfiguration('log_level')],
            output='screen'
        ),
    ])
