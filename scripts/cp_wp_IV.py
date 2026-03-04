#! /usr/bin/env python3
# Copyright 2021 Samsung Research America
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from copy import deepcopy

from geometry_msgs.msg import PoseStamped
from nav2_simple_commander.robot_navigator import BasicNavigator, TaskResult
import rclpy

"""
Basic stock inspection demo. In this demonstration, the expectation
is that there are cameras or RFID sensors mounted on the robots
collecting information about stock quantity and location.
"""

# This fxn converts Euler angles to a quaternion.
# Author: AutomaticAddison.com
import numpy as np # Scientific computing library for Python
 
def get_quaternion_from_euler(roll, pitch, yaw):
  """
  Convert an Euler angle to a quaternion.
   
  Input
    :param roll: The roll (rotation around x-axis) angle in radians.
    :param pitch: The pitch (rotation around y-axis) angle in radians.
    :param yaw: The yaw (rotation around z-axis) angle in radians.
 
  Output
    :return qx, qy, qz, qw: The orientation in quaternion [x,y,z,w] format
  """
  qx = np.sin(roll/2) * np.cos(pitch/2) * np.cos(yaw/2) - np.cos(roll/2) * np.sin(pitch/2) * np.sin(yaw/2)
  qy = np.cos(roll/2) * np.sin(pitch/2) * np.cos(yaw/2) + np.sin(roll/2) * np.cos(pitch/2) * np.sin(yaw/2)
  qz = np.cos(roll/2) * np.cos(pitch/2) * np.sin(yaw/2) - np.sin(roll/2) * np.sin(pitch/2) * np.cos(yaw/2)
  qw = np.cos(roll/2) * np.cos(pitch/2) * np.cos(yaw/2) + np.sin(roll/2) * np.sin(pitch/2) * np.sin(yaw/2)
 
  return [qx, qy, qz, qw]
  
def f2m(feet):
    return feet*0.3048

def main():
    rclpy.init()

    navigator = BasicNavigator()

    # Inspection route, probably read in from a file for a real application
    # from either a map or drive and repeat.
    # [ X-pos, Y-pos, Theta-yaw ]
    # midway: {f2m(13.5/12.0), f2m(5.5), M_PI/2, 50}
    # end {f2m(3), f2m(13), -M_PI/2, 50}
    inspection_route = [
        [f2m(6.0), f2m(3.0) -f2m(13.5/12.0), 0.0],
        [f2m(12.5), 0.0, 3.14]
    ]

    # Set our demo's initial pose
#    initial_pose = PoseStamped()
#    initial_pose.header.frame_id = 'map'
#    initial_pose.header.stamp = navigator.get_clock().now().to_msg()
#    initial_pose.pose.position.x = 0.3
#    initial_pose.pose.position.y = 0.3
#    initial_pose.pose.orientation.z = 0.0
#    initial_pose.pose.orientation.w = 0.0
#    navigator.setInitialPose(initial_pose)

    # Wait for navigation to fully activate
    navigator.waitUntilNav2Active()

    # Send our route
    inspection_points = []
    inspection_pose = PoseStamped()
    inspection_pose.header.frame_id = 'map'
    inspection_pose.header.stamp = navigator.get_clock().now().to_msg()
    #inspection_pose.pose.orientation.z = 0.0
    #inspection_pose.pose.orientation.w = 1.0
    for pt in inspection_route:    
        inspection_pose.pose.position.x = pt[0]
        inspection_pose.pose.position.y = pt[1]
        q = get_quaternion_from_euler(0,0,pt[2])
        inspection_pose.pose.orientation.x = q[0]
        inspection_pose.pose.orientation.y = q[1]      
        inspection_pose.pose.orientation.z = q[2]
        inspection_pose.pose.orientation.w = q[3]  
        inspection_points.append(deepcopy(inspection_pose))
    navigator.followWaypoints(inspection_points)

    # Do something during our route (e.x. AI to analyze stock information or upload to the cloud)
    # Simply the current waypoint ID for the demonstation
    i = 0
    while not navigator.isTaskComplete():
        i += 1
        feedback = navigator.getFeedback()
        if feedback and i % 5 == 0:
            print('Executing current waypoint: ' +
                  str(feedback.current_waypoint + 1) + '/' + str(len(inspection_points)))

    result = navigator.getResult()
    if result == TaskResult.SUCCEEDED:
        print('Inspection complete! Returning to start...')
    elif result == TaskResult.CANCELED:
        print('Inspection was canceled. Returning to start...')
    elif result == TaskResult.FAILED:
        print('Inspection failed! Returning to start...')

    # go back to start
    #initial_pose.header.stamp = navigator.get_clock().now().to_msg()
    #navigator.goToPose(initial_pose)
    #while not navigator.isTaskComplete():
    #    pass

    exit(0)


if __name__ == '__main__':
    main()


# EOF



