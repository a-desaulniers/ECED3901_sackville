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
#include <iostream>
#include <fstream>

// ROS Client Library for C++
#include "rclcpp/rclcpp.hpp"
 
// Message types
#include "std_msgs/msg/string.hpp"
#include "geometry_msgs/msg/pose.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "sensor_msgs/msg/imu.hpp"

#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>

using namespace std::chrono_literals;
using std::placeholders::_1;
using namespace std;


// Create the node class named SquareRoutine
// It inherits rclcpp::Node class attributes and functions
class SquareRoutine : public rclcpp::Node {
  public:
  	static inline double test_vel = 0;
	// Constructor creates a node named Square_Routine. 
	SquareRoutine() : Node("Square_Routine") {
		// Create the subscription
		// The callback function executes whenever data is published to the 'topic' topic.
		subscription_ = this->create_subscription<nav_msgs::msg::Odometry>("odom", 10, std::bind(&SquareRoutine::topic_callback, this, _1));
        imu_sub = this->create_subscription<sensor_msgs::msg::Imu>("imu/data", 10, std::bind(&SquareRoutine::imu_callback, this, _1));
		// Create the publisher
		// Publisher to a topic named "topic". The size of the queue is 10 messages.
		publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel",10);
      
	  	// Create the timer
	  	timer_ = this->create_wall_timer(20ms, std::bind(&SquareRoutine::timer_callback, this)); 	 // Changed to 50Hz 
	
		// Open fout
		fout.open(path);

	}

	~SquareRoutine() {
    	if (fout.is_open()) fout.close();
	}

  private:
 	 // Declaration of subscription_ attribute
	rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr subscription_;

	rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub;
         
	// Declaration of publisher_ attribute      
	rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
	
	// Declaration of the timer_ attribute
	rclcpp::TimerBase::SharedPtr timer_;
	
	// Declaration of Class Variables
	double x_vel = 0.2, ang_vel = 0.2;
	double x_now = 0, x_init = 0, y_now = 0, y_init = 0;
	double d_now = 0, d_aim = 0;
	double rol_now=0, pit_now=0, yaw_now=0, yaw_init=0, yaw_aim=0;
	const double ang_vel_min = 0.5; // minimum angular velocity multiplier
	const double lin_tol=0.01, ang_tol=0.04; // Linear and angular tolerances (1cm and ~2 degrees)
	int ticks = 0; // ticks to wait
	const int wait_ticks=100; // 25 = 500ms
	bool just_moved = 0;

	double imu_yaw_now = 0;
	
	// States enum
	enum State {
		INIT,
		MOVE,
		TURN,
		WAIT,
		STOP
	};
	enum State state = INIT;

	// Output file
	std::ofstream fout;
	string path = "src/eced3901/test_logs/IMU_data.csv";

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

	void imu_callback(const sensor_msgs::msg::Imu::SharedPtr msg) {
		
		tf2::Quaternion q(
			msg->orientation.x,
			msg->orientation.y,
			msg->orientation.z,
			msg->orientation.w
		);
		tf2::Matrix3x3 m(q);
		double dummy;
		m.getRPY(dummy, dummy, imu_yaw_now);
		
		//RCLCPP_INFO(this->get_logger(), "roll: %.3f  pitch: %.3f  yaw: %.3f", rol_now, pit_now, yaw_now);
		RCLCPP_INFO(this->get_logger(), "IMU Yaw: %f", imu_yaw_now);
	}
	
	void timer_callback() {
		if (ticks%50 == 0)
			fout << imu_yaw_now << "," << ticks/50 << ";";
		
		ticks++;

		if (ticks > 500) { // Close after 10s
			if (fout.is_open()) fout.close();
			RCLCPP_INFO(this->get_logger(), "Done! IMU data saved in %s", path.c_str());
			rclcpp::shutdown();
		}

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

	// Get velocity
	//cout << "Velocity: ";
	//cin >> SquareRoutine::test_vel;
	
	// Initialize ROS2
	rclcpp::init(argc, argv);
  
	// Start node and callbacks
	rclcpp::spin(std::make_shared<SquareRoutine>());
 
	// Stop node 
	rclcpp::shutdown();
	return 0;
}



