/*
Code for DT1
V. Sieben
Version 1.0
Date: Feb 4, 2023
License: GNU GPLv3
*/

// Include important C++ header files that provide class
// templates for useful operations.
#include <chrono>		// Timer functions
#include <functional>		// Arithmetic, comparisons, and logical operations
#include <memory>		// Dynamic memory management
#include <string>		// String functions
#include <cmath>

// ROS Client Library for C++
#include "rclcpp/rclcpp.hpp"
 
// Message types
#include "std_msgs/msg/string.hpp"
#include "geometry_msgs/msg/pose.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/odometry.hpp"

#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>

using namespace std::chrono_literals;
using std::placeholders::_1;


// Create the node class named SquareRoutine
// It inherits rclcpp::Node class attributes and functions
class SquareRoutine : public rclcpp::Node {
  public:
	// Constructor creates a node named Square_Routine. 
	SquareRoutine() : Node("Square_Routine") {
		// Create the subscription
		// The callback function executes whenever data is published to the 'topic' topic.
		subscription_ = this->create_subscription<nav_msgs::msg::Odometry>("odom", 10, std::bind(&SquareRoutine::topic_callback, this, _1));
          
		// Create the publisher
		// Publisher to a topic named "topic". The size of the queue is 10 messages.
		publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel",10);
      
	  	// Create the timer
	  	timer_ = this->create_wall_timer(20ms, std::bind(&SquareRoutine::timer_callback, this)); 	 // Changed to 50Hz 
	}

  private:
 	 // Declaration of subscription_ attribute
	rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr subscription_;
         
	// Declaration of publisher_ attribute      
	rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
	
	// Declaration of the timer_ attribute
	rclcpp::TimerBase::SharedPtr timer_;
	
	// Declaration of Class Variables
	double lin_vel_g = 0.3, ang_vel_g = 0.3; // Velocity gains
	double x_now = 0, x_init = 0, y_now = 0, y_init = 0;
	double d_now = 0, d_aim = 0;
	double rol_now=0, pit_now=0, yaw_now=0, yaw_init=0, yaw_aim=0;
	const double lin_vel_min = 0.1; // minimum linear velocity
	const double ang_vel_min = 0.2; // minimum angular velocity
	const double lin_tol=0.01, ang_tol=0.04; // Linear and angular tolerances (1cm and ~2 degree)
	int ticks = 0; // ticks to wait
	int move_count = 0; // Number of movements, used to determine stop state
	const int wait_ticks=100; // 25 = 500ms, 50 = 1s
	bool just_moved = 0;
	
	// States enum
	enum State {
		INIT,
		MOVE,
		TURN,
		WAIT, 
		STOP
	};
	enum State state = INIT;

	// Functions
	void topic_callback(const nav_msgs::msg::Odometry::SharedPtr msg) {
		x_now = msg->pose.pose.position.x;
		y_now = msg->pose.pose.position.y;
		
		tf2::Quaternion q(
			msg->pose.pose.orientation.x,
			msg->pose.pose.orientation.y,
			msg->pose.pose.orientation.z,
			msg->pose.pose.orientation.w
		);
		tf2::Matrix3x3 m(q);
		m.getRPY(rol_now, pit_now, yaw_now);
		
		//DEBUG PRINT SYNTAX 
		//RCLCPP_INFO(this->get_logger(), "roll: %.3f  pitch: %.3f  yaw: %.3f", rol_now, pit_now, yaw_now);
	}
	
	void timer_callback() {
		geometry_msgs::msg::Twist msg;
		
		// State machine
		switch (state) {
			case INIT: {
				move_distance(1.0);
				state = MOVE;
				break;
			}
			case MOVE: {
				// Calculate distance travelled from initial
				d_now =	pow( pow(x_now - x_init, 2) + pow(y_now - y_init, 2), 0.5 );
				double d_err = d_aim - d_now;
				if (d_err < lin_tol) { // Check if within tolerance
					msg.linear.x = 0;
					msg.angular.z = 0;
					if (++move_count >=4) { // Check if square finished
						state = STOP;
						break;
					}
					ticks = wait_ticks;
					state = WAIT;
					just_moved = 1;
				} else {
					// Compute velocity
					double vel = lin_vel_g * d_err;
					if (vel < lin_vel_min) vel = lin_vel_min;
					RCLCPP_INFO(this->get_logger(), "Linear Velocity: %f", vel);
					msg.linear.x = vel; 
					msg.angular.z = 0;
				}
				//RCLCPP_INFO(this->get_logger(), "Moving. Ang: %f", yaw_now);
				break;
			}
			case TURN: {
				// Calculate rotation
				double rot_now = atan2(sin(yaw_now - yaw_init), cos(yaw_now - yaw_init)); // arctan(tan(x)) normalizes the angle into the range (-pi, pi)
				double ang_err = atan2(sin(yaw_aim - rot_now), cos(yaw_aim - rot_now)); // CANT HANDLE CW TURNS
				//RCLCPP_INFO(this->get_logger(), "Turning. Ang: %f, Ang err: %f", yaw_now, ang_err);
				if (ang_err < ang_tol) { // Close enough
					msg.angular.z = 0;
					ticks = wait_ticks;
					state = WAIT;
					just_moved = 0;
					
				} else {
					// Compute angular velocity
					// TODO: Eliminate directional bias, implement overshoot correction?
					double vel = ang_err * ang_vel_g;
					if (vel < ang_vel_min) vel = ang_vel_min;
					RCLCPP_INFO(this->get_logger(), "Angular Velocity: %f", vel);
					msg.angular.z = vel;
				}
				break;
			}
			case WAIT: {
				msg.linear.x = 0;
				msg.angular.z = 0;
				//RCLCPP_INFO(this->get_logger(), "waiting. Ticks: %d", ticks );
				if ((ticks--) <= 0) { // Still waiting?
					state = just_moved?TURN:MOVE;
					sequence_statemachine();	
				}
				break;
			}
			case STOP: {
				msg.linear.x = 0;
				msg.angular.z = 0;
			}
		}
		//RCLCPP_INFO(this->get_logger(), "linear: %f, %f, %f, angular: %f, %f, %f", msg.linear.x, msg.linear.y, msg.linear.z, msg.angular.x, msg.angular.y, msg.angular.z);
		// Publish msg
		publisher_->publish(msg);
	}
	
	void sequence_statemachine() {
			switch(state) {
			  case MOVE:
			    move_distance(1.0);
			    break;
			  case TURN:
			    turn_rads(M_PI/2);
			    break;
			  default:
			    break;
			}			
	}
	
	// Set the initial position as where robot is now and put new d_aim in place	
	void move_distance(double distance) {
		d_aim = distance;
		x_init = x_now;
		y_init = y_now;		
		//state = MOVE;	
	}
	
	void turn_rads(double rads) {
		yaw_aim = atan2(sin(rads), cos(rads)); // Normalize angle in (-pi, pi)
		yaw_init = yaw_now;
		//state = TURN;
	}
	

	
};
    	


//------------------------------------------------------------------------------------
// Main code execution
int main(int argc, char * argv[]) {
	// Initialize ROS2
	rclcpp::init(argc, argv);
  
	// Start node and callbacks
	rclcpp::spin(std::make_shared<SquareRoutine>());
 
	// Stop node 
	rclcpp::shutdown();
	return 0;
}



