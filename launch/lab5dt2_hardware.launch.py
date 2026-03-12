import os
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, RegisterEventHandler
from launch.event_handlers import OnProcessExit
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare

def generate_launch_description():
    # 1. Find the dalmotor launch file
    # This finds the file even if it's hidden in /opt/ros/
    dalmotor_launch_dir = FindPackageShare(package='dalmotor').find('dalmotor')
    repo_path = '/home/student/ros2_ws/src/eced3901'


    # 2. Include the robot hardware launch
    start_robot_hw = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(dalmotor_launch_dir, 'launch', 'robot.launch.py')
        )
    )

    # 3. Add your C++ controller node
    start_square_routine = Node(
    package='eced3901',
    executable='dt1',
    name='square_routine',
    output='screen',
  )

    # 4. Define the Map Saver (The same logic we used before)
    save_map_cmd = Node(
        package='nav2_map_server',
        executable='map_saver_cli',
        arguments=['-f', os.path.join(repo_path, 'maps', 'dt2_map_hardware')]
    )

    # 5. Trigger save when your controller finishes
    save_map_on_exit = RegisterEventHandler(
    event_handler=OnProcessExit(
        target_action=start_square_routine, # This is your C++ node from earlier
        on_exit=[save_map_cmd],
    )
  )

    return LaunchDescription([
        start_robot_hw,
        start_square_routine,
        save_map_on_exit
    ])