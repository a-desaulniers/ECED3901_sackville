// Author: Alexandre DesAulniers
// ROS2 Serial Ultrasonic Driver + Servo Trigger + RViz2 Visualization
// Written Mar 2nd 2026 / Cleaned up version

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <limits>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"
#include "std_msgs/msg/string.hpp"
#include "sensor_msgs/msg/range.hpp"

using namespace std::chrono_literals;

class SerialBridgeNode : public rclcpp::Node {
public:
    SerialBridgeNode() : Node("serial_bridge_node") {
        // Create 4 Int32 publishers
        int_publishers_.push_back(this->create_publisher<std_msgs::msg::Int32>("int32_rear_right", 10));
        int_publishers_.push_back(this->create_publisher<std_msgs::msg::Int32>("int32_rear_left", 10));
        int_publishers_.push_back(this->create_publisher<std_msgs::msg::Int32>("int32_front_left", 10));
        int_publishers_.push_back(this->create_publisher<std_msgs::msg::Int32>("int32_front_right", 10));
        
        // Create 4 Range publishers for RViz
        range_publishers_.push_back(this->create_publisher<sensor_msgs::msg::Range>("range_rear_right", 10));
        range_publishers_.push_back(this->create_publisher<sensor_msgs::msg::Range>("range_rear_left", 10));
        range_publishers_.push_back(this->create_publisher<sensor_msgs::msg::Range>("range_front_left", 10));
        range_publishers_.push_back(this->create_publisher<sensor_msgs::msg::Range>("range_front_right", 10));
        
        // Create subscriber for servo commands
        servo_sub_ = this->create_subscription<std_msgs::msg::String>(
            "servo_command", 10, std::bind(&SerialBridgeNode::servo_callback, this, std::placeholders::_1));
        
        // Startup info
        RCLCPP_INFO(this->get_logger(), "========================================");
        RCLCPP_INFO(this->get_logger(), "Serial Bridge Node started");
        RCLCPP_INFO(this->get_logger(), "Reading 4 sensors from /dev/ttyUSB4");
        RCLCPP_INFO(this->get_logger(), "Int32 topics: int32_rear_right, int32_rear_left, int32_front_left, int32_front_right");
        RCLCPP_INFO(this->get_logger(), "Range topics: range_rear_right, range_rear_left, range_front_left, range_front_right");
        RCLCPP_INFO(this->get_logger(), "Subscribed to: servo_command (send '1' or 'trigger')");
        RCLCPP_INFO(this->get_logger(), "========================================");
        RCLCPP_INFO(this->get_logger(), "Live data stream:");
        RCLCPP_INFO(this->get_logger(), "----------------------------------------");
        
        // Open serial port
        serial_port_ = open("/dev/ttyUSB4", O_RDWR | O_NOCTTY);
        
        if (serial_port_ < 0) {
            RCLCPP_ERROR(this->get_logger(), "Error opening serial port: %s", strerror(errno));
            return;
        }
        
        // Configure serial port
        struct termios tty;
        if(tcgetattr(serial_port_, &tty) != 0) {
            RCLCPP_ERROR(this->get_logger(), "Error from tcgetattr: %s", strerror(errno));
            return;
        }

        cfsetospeed(&tty, B115200);
        cfsetispeed(&tty, B115200);
        
        tty.c_cflag |= (CLOCAL | CREAD);
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;
        tty.c_cflag &= ~PARENB;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;
        
        tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        tty.c_iflag &= ~(IXON | IXOFF | IXANY);
        tty.c_iflag &= ~(INLCR | ICRNL | IGNCR);
        tty.c_oflag &= ~OPOST;
        
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 1;
        
        tcsetattr(serial_port_, TCSANOW, &tty);
        tcflush(serial_port_, TCIOFLUSH);
        
        // Start reading timer
        timer_ = this->create_wall_timer(10ms, std::bind(&SerialBridgeNode::read_serial, this));
    }

    ~SerialBridgeNode() { 
        if (serial_port_ >= 0) {
            close(serial_port_);
            RCLCPP_INFO(this->get_logger(), "Serial port closed");
        }
    }

private:
    void servo_callback(const std_msgs::msg::String::SharedPtr msg) {
        RCLCPP_INFO(this->get_logger(), "[SERVO] Command received: '%s'", msg->data.c_str());
        
        if (msg->data == "1" || msg->data == "trigger") {
            unsigned char trigger = '1';
            int n = write(serial_port_, &trigger, 1);
            if (n == 1) {
                tcdrain(serial_port_);
                RCLCPP_INFO(this->get_logger(), "[SERVO] Trigger sent to Arduino");
            } else {
                RCLCPP_ERROR(this->get_logger(), "[SERVO] Failed to send, errno=%d", errno);
            }
        }
    }

    void publish_range_message(uint8_t sensor_id, float distance_cm) {
        auto range_msg = sensor_msgs::msg::Range();
        
        float distance_m = distance_cm / 100.0;
        
        range_msg.header.stamp = this->get_clock()->now();
        range_msg.header.frame_id = "ultrasonic_" + std::to_string(sensor_id + 1);
        range_msg.radiation_type = sensor_msgs::msg::Range::ULTRASOUND;
        range_msg.field_of_view = 0.5236;
        range_msg.min_range = 0.02;
        range_msg.max_range = 4.0;
        
        if (distance_cm > 0) {
            range_msg.range = distance_m;
        } else {
            range_msg.range = std::numeric_limits<float>::infinity();
        }
        
        if (sensor_id < range_publishers_.size()) {
            range_publishers_[sensor_id]->publish(range_msg);
        }
    }

    void read_serial() {
        static std::string buffer;
        char buf[64];
        
        int n = read(serial_port_, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            buffer.append(buf);
            
            size_t pos;
            while ((pos = buffer.find('\n')) != std::string::npos) {
                std::string line = buffer.substr(0, pos);
                buffer.erase(0, pos + 1);
                
                // Clean up line
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                
                if (line.empty()) {
                    continue;
                }
                
                // Show raw data
                RCLCPP_INFO(this->get_logger(), "[DATA] %s", line.c_str());
                
                // Handle leading comma
                if (!line.empty() && line[0] == ',') {
                    line = "0" + line;
                }
                
                // Parse CSV
                std::vector<int> distances;
                std::stringstream ss(line);
                std::string token;
                
                while (std::getline(ss, token, ',')) {
                    if (token.empty()) {
                        distances.push_back(0);
                    } else {
                        try {
                            distances.push_back(std::stoi(token));
                        } catch (...) {
                            distances.push_back(0);
                        }
                    }
                }
                
                // Pad to 4 values
                while (distances.size() < 4) {
                    distances.push_back(0);
                }
                
                // Publish to topics
                for (size_t i = 0; i < 4 && i < distances.size(); i++) {
                    // Int32 message
                    auto int_msg = std_msgs::msg::Int32();
                    int_msg.data = distances[i];
                    int_publishers_[i]->publish(int_msg);
                    
                    // Range message for RViz
                    publish_range_message(i, static_cast<float>(distances[i]));
                }
            }
        }
    }

    std::vector<rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr> int_publishers_;
    std::vector<rclcpp::Publisher<sensor_msgs::msg::Range>::SharedPtr> range_publishers_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr servo_sub_;
    rclcpp::TimerBase::SharedPtr timer_;
    int serial_port_;
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<SerialBridgeNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}