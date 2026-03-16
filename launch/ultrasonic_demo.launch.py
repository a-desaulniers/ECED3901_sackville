# Ultrasonic Demo Launch File
# Written by Alexandre DesAulniers 
# March 16th 2026

# ~/ros2_ws/src/eced3901/launch/ultrasonic_demo.launch.py
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        # Connect odom to base_link (assuming robot at origin)
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            arguments=['0', '0', '0', '0', '0', '0', 'odom', 'base_link']
        ),
        
        # Your serial bridge node
        Node(
            package='eced3901',
            executable='serial_bridge',
            name='serial_bridge',
            output='screen'
        ),
        
        # Position sensors around base_link
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            arguments=['0.2', '0.1', '0', '0', '0', '0', 'base_link', 'ultrasonic_1']
        ),
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            arguments=['0.2', '-0.1', '0', '0', '0', '0', 'base_link', 'ultrasonic_2']
        ),
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            arguments=['-0.2', '0.1', '0', '0', '0', '0', 'base_link', 'ultrasonic_3']
        ),
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            arguments=['-0.2', '-0.1', '0', '0', '0', '0', 'base_link', 'ultrasonic_4']
        ),
    ])

    # arguments=['x', 'y', 'z', 'roll', 'pitch', 'yaw', 'parent_frame', 'child_frame']
    # odom ← [dynamic] ← base_link ← [static] ← ultrasonic_4
    # we'll place it with these movements, and it will stick to the robot on odom (apparently)
