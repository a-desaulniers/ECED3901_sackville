#! /usr/bin/env python3
# cp_left_wp.py - COMPLETE UPDATED FILE
from copy import deepcopy
import time  # Added for pause

from geometry_msgs.msg import PoseStamped
from nav2_simple_commander.robot_navigator import BasicNavigator, TaskResult
import rclpy
import numpy as np

def get_quaternion_from_euler(roll, pitch, yaw):
    """
    Convert an Euler angle to a quaternion.
    """
    qx = np.sin(roll/2) * np.cos(pitch/2) * np.cos(yaw/2) - np.cos(roll/2) * np.sin(pitch/2) * np.sin(yaw/2)
    qy = np.cos(roll/2) * np.sin(pitch/2) * np.cos(yaw/2) + np.sin(roll/2) * np.cos(pitch/2) * np.sin(yaw/2)
    qz = np.cos(roll/2) * np.cos(pitch/2) * np.sin(yaw/2) - np.sin(roll/2) * np.sin(pitch/2) * np.cos(yaw/2)
    qw = np.cos(roll/2) * np.cos(pitch/2) * np.cos(yaw/2) + np.sin(roll/2) * np.sin(pitch/2) * np.sin(yaw/2)
    return [qx, qy, qz, qw]

def f2m(feet):
    return feet * 0.3048

def main():
    rclpy.init()

    navigator = BasicNavigator()

    # Inspection route (left side)
    inspection_route = [
        [f2m(6.0), f2m(3.0) - f2m(13.5/12.0), 0.0],
        [f2m(9.0), f2m(0.0), 0.0],
        [f2m(12.5), f2m(2.0), -1.57],
        [f2m(12.5), -f2m(0.5), -1.57],
        [f2m(11.6), -f2m(0.5), 3.14]
    ]

    # Return route
    reverse_route = [
        [f2m(6.0), f2m(3.0) - f2m(13.5/12.0), 3.14],
        [-f2m(0.5), -f2m(0.5), 3.14]
    ]

    print("Waiting for navigation to activate...")
    navigator.waitUntilNav2Active()
    print("Navigation activated!")

    # --- Outward journey ---
    print("Starting outward journey...")
    inspection_points = []
    inspection_pose = PoseStamped()
    inspection_pose.header.frame_id = 'map'
    for i, pt in enumerate(inspection_route):
        inspection_pose.header.stamp = navigator.get_clock().now().to_msg()
        inspection_pose.pose.position.x = pt[0]
        inspection_pose.pose.position.y = pt[1]
        q = get_quaternion_from_euler(0, 0, pt[2])
        inspection_pose.pose.orientation.x = q[0]
        inspection_pose.pose.orientation.y = q[1]
        inspection_pose.pose.orientation.z = q[2]
        inspection_pose.pose.orientation.w = q[3]
        inspection_points.append(deepcopy(inspection_pose))
        print(f"Added waypoint {i+1}: x={pt[0]:.2f}, y={pt[1]:.2f}, yaw={pt[2]:.2f}")

    navigator.followWaypoints(inspection_points)

    i = 0
    while not navigator.isTaskComplete():
        i += 1
        feedback = navigator.getFeedback()
        if feedback and i % 5 == 0:
            print('Executing current waypoint: ' +
                  str(feedback.current_waypoint + 1) + '/' + str(len(inspection_points)))

    result = navigator.getResult()
    if result == TaskResult.SUCCEEDED:
        print('Inspection complete!')
    elif result == TaskResult.CANCELED:
        print('Inspection was canceled.')
    elif result == TaskResult.FAILED:
        print('Inspection failed!')

    # --- Pause to let SLAM/AMCL settle after rotation ---
    print("Pausing for 2 seconds to let localization stabilize...")
    time.sleep(2.0)

    # --- Return journey ---
    print("Starting return journey...")
    inspection_points = []
    for i, pt in enumerate(reverse_route):
        inspection_pose.header.stamp = navigator.get_clock().now().to_msg()
        inspection_pose.pose.position.x = pt[0]
        inspection_pose.pose.position.y = pt[1]
        q = get_quaternion_from_euler(0, 0, pt[2])
        inspection_pose.pose.orientation.x = q[0]
        inspection_pose.pose.orientation.y = q[1]
        inspection_pose.pose.orientation.z = q[2]
        inspection_pose.pose.orientation.w = q[3]
        inspection_points.append(deepcopy(inspection_pose))
        print(f"Added waypoint {i+1}: x={pt[0]:.2f}, y={pt[1]:.2f}, yaw={pt[2]:.2f}")

    navigator.followWaypoints(inspection_points)

    i = 0
    while not navigator.isTaskComplete():
        i += 1
        feedback = navigator.getFeedback()
        if feedback and i % 5 == 0:
            print('Executing current waypoint: ' +
                  str(feedback.current_waypoint + 1) + '/' + str(len(inspection_points)))

    print("Mission complete!")
    exit(0)

if __name__ == '__main__':
    main()