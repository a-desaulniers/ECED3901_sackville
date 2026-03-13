#!/usr/bin/env python3
# I HATE VIBE CODING I HATE VIBE CODING I HATE VIBE CODING I HATE VIBE CODING

import rclpy
from rclpy.node import Node
from rclpy.callback_groups import ReentrantCallbackGroup
from rclpy.executors import MultiThreadedExecutor
from std_msgs.msg import String, Float32
from nav2_simple_commander.robot_navigator import BasicNavigator, TaskResult
from geometry_msgs.msg import PoseStamped
import numpy as np
from enum import Enum
from copy import deepcopy

class State(Enum):
    INIT = 1
    GO = 2
    DUMP = 3
    COME = 4
    SAVE = 5


class NavigatorNode(Node):

    def __init__(self):
        super().__init__('navigator_node')

        # Declare routes
        self.inspection_route = [
            [f2m(6.0), f2m(3.0) -f2m(13.5/12.0), 0.0],
            [f2m(9.0), f2m(0.0), 0.0],
            [f2m(12.5), f2m(2.0), -1.57], # Aim at cargo
            [f2m(12.5), -f2m(0.5), -1.57], # Ram cargo (Hopefully acquire)
            [f2m(11.6), -f2m(0.5), 3.14] # Look away to dump 1st cargo
        ]
        self.reverse_route = [
            [f2m(6.0), f2m(3.0) -f2m(13.5/12.0), 3.14],
            [-f2m(0.5), -f2m(0.5), 3.14]
        ]
        # Declare other things
        self.navigator = BasicNavigator()
        self.state = State.INIT
        self.inspection_points = []

        # This allows callbacks in this group to run in parallel
        self.group = ReentrantCallbackGroup()

        # 1. The "Data Parser" (Topic Callback)
        self.data_sub = self.create_subscription(
            Float32, 
            'ultrasonic_1', 
            self.listener_callback, 
            10, 
            callback_group=self.group
        )
        self.latest_data = 0.0

        # Wait for navigation to fully activate
        self.navigator.waitUntilNav2Active()
        self.inspection_pose = PoseStamped()
        self.inspection_pose.header.frame_id = 'map'
        self.inspection_pose.header.stamp = self.navigator.get_clock().now().to_msg()

        # 2. The "Navigator" (Timer Callback)
        # Replaces your while loop; runs every 0.1 seconds (10Hz)
        self.nav_timer = self.create_timer(
            0.1, 
            self.navigator_loop, 
            callback_group=self.group
        )

    def listener_callback(self, msg):
        self.latest_data = msg.data
        self.get_logger().info(f'Parsed new data: {self.latest_data}')

    def navigator_loop(self):
        # Your "Blocking" logic goes here. 
        # Even if this takes 0.05s, the listener_callback can still trigger.
        #self.get_logger().info(f'Navigating using data: {self.latest_data}')
        # Do navigation math...
        match self.state:
            case State.INIT:
                self.get_logger().info('Initializing...')
                # Set forward route
                self.inspection_points = []
                for pt in self.inspection_route:    
                    self.inspection_pose.pose.position.x = pt[0]
                    self.inspection_pose.pose.position.y = pt[1]
                    q = get_quaternion_from_euler(0,0,pt[2])
                    self.inspection_pose.pose.orientation.x = q[0]
                    self.inspection_pose.pose.orientation.y = q[1]      
                    self.inspection_pose.pose.orientation.z = q[2]
                    self.inspection_pose.pose.orientation.w = q[3]  
                    self.inspection_points.append(deepcopy(self.inspection_pose))
                self.state = State.GO
                self.navigator.followWaypoints(self.inspection_points)
                
            case State.GO:
                if not self.navigator.isTaskComplete(): # Still goin'
                    self.get_logger().info('Going to destination...')
                    feedback = self.navigator.getFeedback()
                    if feedback:
                        self.get_logger().info(f'At waypoint: {feedback.current_waypoint}')
                else: # We're done here
                    result = self.navigator.getResult()
                    if result == TaskResult.SUCCEEDED:
                        self.get_logger().info('Arrived at north port!')
                        self.state = State.DUMP
                    else:
                        self.get_logger().error('Navigation failed!')
            case State.DUMP:
                self.get_logger().info('Dropping cargo...')
                # Implement later kek

                if True: # When done, setup return journey
                    self.inspection_points = []
                    for pt in self.reverse_route:    
                        self.inspection_pose.pose.position.x = pt[0]
                        self.inspection_pose.pose.position.y = pt[1]
                        q = get_quaternion_from_euler(0,0,pt[2])
                        self.inspection_pose.pose.orientation.x = q[0]
                        self.inspection_pose.pose.orientation.y = q[1]      
                        self.inspection_pose.pose.orientation.z = q[2]
                        self.inspection_pose.pose.orientation.w = q[3]  
                        self.inspection_points.append(deepcopy(self.inspection_pose))
                    self.state = State.COME
                    self.navigator.followWaypoints(self.inspection_points)
                
            case State.COME:
                if not self.navigator.isTaskComplete():
                    self.get_logger().info('Returning home...')
                    feedback = self.navigator.getFeedback()
                    if feedback:
                        self.get_logger().info(f'At waypoint: {feedback.current_waypoint}')
                else: # We're done here
                    result = self.navigator.getResult()
                    if result == TaskResult.SUCCEEDED:
                        self.get_logger().info('Arrived at north port!')
                        self.state = State.DUMP
                    else:
                        self.get_logger().error('Navigation failed!')
                    rclpy.shutdown()
                
            case State.SAVE:
                self.get_logger().info('Saving lil guy...')

# Some functions:

def f2m(feet): # Feet to meters conversion
    return feet*0.3048

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

def main(args=None):
    rclpy.init(args=args)
    node = NavigatorNode()

    # Use MultiThreadedExecutor so the timer and sub don't wait for each other
    executor = MultiThreadedExecutor()
    executor.add_node(node)

    try:
        executor.spin()
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
