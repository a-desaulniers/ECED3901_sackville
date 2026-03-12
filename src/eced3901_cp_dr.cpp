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


struct Action {
	double x; // Target x position
	double y; // Target y position
	double yaw; // Target yaw after movement
	int wait;   // Wait ticks after movement (50 ticks/s) (NOT IMPLEMENTED)
};

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
	double x_start = f2m(1), y_start = f2m(-3), yaw_start = 0; // XYyaw at start of routine
	double x_now = 0, x_init = x_start, y_now = 0, y_init = y_start;
	double d_aim = 0;
	double rol_now=0, pit_now=0, yaw_now=0, yaw_init=yaw_start, yaw_aim=0;
	double lin_vel_now = 0, ang_vel_now = 0;
	int move_count = 0; // Number of movements, used to determine stop state
	int ticks = 0, settle_ticks = 25; // ticks to wait
	bool just_moved = 0;

	// Tweakables
	const double lin_vel_g = 0.3, ang_vel_g = 0.3; // Velocity gains
	const double lin_vel_min = 0.1, lin_acc_max = 0.002; // minimum linear velocity, max acceleration per tick (0.01 = 0.5 m/s/s).
	const double ang_vel_min = 0.05, ang_acc_max = 0.005; // minimum angular velocity (WAS 0.2, 0.05 might be too slow for hardware, but DRASTICALLY IMPROVED ACCURACY), max accel (0.005 = 0.25 rad/s/s)
	const double lin_tol=0.01, ang_tol=0.02; // Linear and angular tolerances (1cm and ~1 degree)
	const int wait_ticks=25; // 25 = 500ms, 50 = 1s
	
	// States enum
	enum State {
		INIT,
		ALIGN,
		MOVE,
		TURN,
		WAIT, 
		STOP
	};
	enum State state = INIT;
	enum State prev_state = INIT;
	
	// Actions/waypoints
	Action steps[5] = { // Steps for coastal path // Swapped x and y
		{f2m(5.5), f2m(-3), -M_PI/2, 50},
		{f2m(5.5), f2m(-6+13.5/12.0), 0, 50},
		{f2m(8.5), f2m(-6+13.5/12.0), M_PI/2, 50},
		{f2m(8.5), f2m(-3), 0, 50},
		{f2m(13), f2m(-3), -M_PI, 50}
	};
	unsigned int step_count = 0;

	// Initialization correction
	bool got_init = false;

	// Functions
	double f2m(double feet) { // Feet to meters conversion
		return feet * 0.3048;
	}

	void topic_callback(const nav_msgs::msg::Odometry::SharedPtr msg) {
		// Calculate rpy
		tf2::Quaternion q(
			msg->pose.pose.orientation.x,
			msg->pose.pose.orientation.y,
			msg->pose.pose.orientation.z,
			msg->pose.pose.orientation.w
		);
		tf2::Matrix3x3 m(q);
		m.getRPY(rol_now, pit_now, yaw_now);
		
		// Fix initial offset
		if (!got_init) {
			x_start -= msg->pose.pose.position.x;
			y_start -= msg->pose.pose.position.y;
			yaw_start = atan2(sin(yaw_start - yaw_now), cos(yaw_start - yaw_now));
			got_init = true;
			RCLCPP_INFO(this->get_logger(), "init x: %f, init y: %f, init yaw: %f, seen yaw: %f", x_start, y_start, yaw_start, atan2(sin(yaw_now + yaw_start), cos(yaw_now + yaw_start)) );
		}
		
		// Flip X and Y due to IMU fuckery
		x_now = msg->pose.pose.position.x + x_start; // Added start offset
		y_now = msg->pose.pose.position.y + y_start; // "
		
		yaw_now = atan2(sin(yaw_now + yaw_start), cos(yaw_now + yaw_start)); // Start offset

		//DEBUG PRINT SYNTAX 
		//RCLCPP_INFO(this->get_logger(), "roll: %.3f  pitch: %.3f  yaw: %.3f", rol_now, pit_now, yaw_now);
		//RCLCPP_INFO(this->get_logger(), "yaw: %f", yaw_now); // Print Yaw
		//RCLCPP_INFO(this->get_logger(), "x: %f, y: %f", x_now, y_now);
	}
	
	void timer_callback() {
		geometry_msgs::msg::Twist msg;
		
		// State machine
		switch (state) {
			case INIT: {
				// Wait for robot to be ready
				if (this->count_publishers("/odom") == 0) {
   					RCLCPP_INFO(this->get_logger(), "Waiting for hardware (/odom)...");
    				break;
				}
				RCLCPP_INFO(this->get_logger(), "Hardware ready! Starting routine.");

				//move_distance(1.0);
				state = ALIGN;
				break;
			}
			case ALIGN: {
				double yaw_tar = atan2(steps[step_count].y - y_now, steps[step_count].x - x_now); // Target yaw
				RCLCPP_INFO(this->get_logger(), "Aligning. x: %f, y: %f,   xtar: %f, ytar: %f", x_now, y_now, steps[step_count].x, steps[step_count].y);
				//RCLCPP_INFO(this->get_logger(), "Aligning. yaw: %f, target: %f", yaw_now, yaw_tar);
				if (!calc_ang_vel(yaw_tar, msg)) break; // Movement incomplete
				// Movement complete
				prev_state = state;
				ticks = settle_ticks;
				state = WAIT;

				break;
			}
			case MOVE: {
				// Calculate distance target
				double d_tar = pow( pow(steps[step_count].x - x_init, 2) + pow(steps[step_count].y - y_init, 2), 0.5);
				//RCLCPP_INFO(this->get_logger(), "Moving. d_tar: %f", d_tar);
				if (!calc_vel(d_tar, msg)) break;
				prev_state = state;
				ticks = settle_ticks;
				state = WAIT;
				break;
			}
			case TURN: {
				//RCLCPP_INFO(this->get_logger(), "Turning. yaw: %f", yaw_now);
				if (!calc_ang_vel(steps[step_count].yaw, msg)) break; // Movement incomplete
				// Movement complete
				prev_state = state;
				ticks = settle_ticks;
				state = WAIT;
				break;
			}
			case WAIT: {
				msg.linear.x = 0;
				msg.angular.z = 0;
				//RCLCPP_INFO(this->get_logger(), "waiting. Ticks: %d", ticks );
				if ((ticks--) > 0) break; // Still waiting?
				switch (prev_state) { // Evil nested switch
					case ALIGN:
						state = MOVE;
						break;
					case MOVE:
						state = TURN;
						break;
					case TURN:
						step_count++;
						if (step_count >= 5) state = STOP; // 5 WILL NEED TO BE CHANGED
						else state = ALIGN;
						break;
					default:
						break;
				}	
				x_init = x_now;
				y_init = y_now;
				yaw_init = yaw_now;	
				break;
			}
			case STOP: {
				msg.linear.x = 0;
				msg.angular.z = 0;
				publisher_->publish(msg);
				rclcpp::shutdown();
			}
		}
		//RCLCPP_INFO(this->get_logger(), "linear: %f, %f, %f, angular: %f, %f, %f", msg.linear.x, msg.linear.y, msg.linear.z, msg.angular.x, msg.angular.y, msg.angular.z);
		// Publish msg
		publisher_->publish(msg);
	}

	char calc_ang_vel(double target, geometry_msgs::msg::Twist &msg) { // PI controller to turn the robot to the target angle. Returns 1 when complete.
		// Calculate rotation
		//double rot_now = atan2(sin(yaw_now - yaw_init), cos(yaw_now - yaw_init)); // arctan(tan(x)) normalizes the angle into the range (-pi, pi)
		double ang_err = atan2(sin(target - yaw_now), cos(target - yaw_now));
		//RCLCPP_INFO(this->get_logger(), "Turning. Ang: %f, Ang err: %f", yaw_now, ang_err);
		if (abs(ang_err) < ang_tol) { // Close enough
			msg.angular.z = 0;
			//ticks = wait_ticks;
			//state = WAIT;
			ang_vel_now = 0;
			//just_moved = 0;
			return 1;
		} 
		// Compute angular velocity
		double target_vel = ang_err * ang_vel_g;
		double vel_sign = ang_err>0?1:-1;
		double vel = ang_vel_now;
		// Easing
		if (abs(target_vel) < ang_vel_min) vel = vel_sign * ang_vel_min;
		else if (abs(target_vel) > abs(ang_vel_now)) vel += vel_sign * ang_acc_max;
		else vel = target_vel;
		//RCLCPP_INFO(this->get_logger(), "Angular Velocity: %f", vel);
		msg.angular.z = vel;
		ang_vel_now = vel;
		return 0;
	}
	
	char calc_vel(double target, geometry_msgs::msg::Twist &msg) { // PI controller to move straight to target
		// Calculate distance travelled from initial
		double d_now =	pow( pow(x_now - x_init, 2) + pow(y_now - y_init, 2), 0.5 );
		double d_err = target - d_now;
		if (d_err < lin_tol) { // Check if within tolerance
			msg.linear.x = 0;
			msg.angular.z = 0;
			lin_vel_now = 0;
			//ticks = wait_ticks;
			//state = WAIT;
			//just_moved = 1;
			return 1;
		}
		// Calculate veering
		double ang_err = atan2(sin(yaw_now - yaw_init), cos(yaw_now - yaw_init)); 
		// Compute velocity
		double target_vel = lin_vel_g * d_err;
		double vel = lin_vel_now;
		// Easing
		if (target_vel < lin_vel_min) vel = lin_vel_min;
		else if (target_vel > vel) vel += lin_acc_max;
		else vel = target_vel;
		//RCLCPP_INFO(this->get_logger(), "Linear Velocity: %f", vel);
		msg.linear.x = vel; 
		// Compute veering correction
		double veer = 0;
		if (abs(ang_err) > ang_tol) veer = -(ang_err>0?1:-1) * ang_vel_min; // Gently correct veering
		msg.angular.z = veer;
		lin_vel_now = vel;
		//RCLCPP_INFO(this->get_logger(), "Moving. Ang: %f", yaw_now);
		return 0;
	}
	
	// Set the initial position as where robot is now and put new d_aim in place	
	void move_distance(double distance) {
		d_aim = distance;
		x_init = x_now;
		y_init = y_now;
		yaw_init = yaw_now;	
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
	//rclcpp::shutdown();
	return 0;
}



