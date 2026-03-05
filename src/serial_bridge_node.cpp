// Author: Alexandre DesAulniers
// ROS2 Serial Ultrasonic Driver + Servo Trigger + Console Output
// Written Mar 2nd 2026 / Updated with live console view

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

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

class SerialBridgeNode : public rclcpp::Node {
public:
    SerialBridgeNode() : Node("serial_bridge_node") {
        // Create 4 publishers for ultrasonic sensors
        publishers_.push_back(this->create_publisher<std_msgs::msg::Int32>("ultrasonic_1", 10));
        publishers_.push_back(this->create_publisher<std_msgs::msg::Int32>("ultrasonic_2", 10));
        publishers_.push_back(this->create_publisher<std_msgs::msg::Int32>("ultrasonic_3", 10));
        publishers_.push_back(this->create_publisher<std_msgs::msg::Int32>("ultrasonic_4", 10));
        
        // Create subscriber for servo commands
        servo_sub_ = this->create_subscription<std_msgs::msg::String>(
            "servo_command", 10, std::bind(&SerialBridgeNode::servo_callback, this, std::placeholders::_1));
        
        RCLCPP_INFO(this->get_logger(), "========================================");
        RCLCPP_INFO(this->get_logger(), "Serial Bridge Node started");
        RCLCPP_INFO(this->get_logger(), "Reading 4 sensors from /dev/ttyUSB0");
        RCLCPP_INFO(this->get_logger(), "Publishing to: ultrasonic_1 through ultrasonic_4");
        RCLCPP_INFO(this->get_logger(), "Subscribed to: servo_command (send '1' or 'trigger')");
        RCLCPP_INFO(this->get_logger(), "========================================");
        RCLCPP_INFO(this->get_logger(), "Live data stream (like cat /dev/ttyUSB0):");
        RCLCPP_INFO(this->get_logger(), "----------------------------------------");
        
        // Open connection to serial device (r/w)
        serial_port_ = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY);
        
        if (serial_port_ < 0) {
            RCLCPP_ERROR(this->get_logger(), "Error opening serial port: %s", strerror(errno));
            return;
        }
        
        // Configure serial port for 115200 baud
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
        
        // Flush any initial shit
        tcflush(serial_port_, TCIOFLUSH);
        
        // Timer loop to read serial
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
                RCLCPP_INFO(this->get_logger(), "[SERVO] ✓ Trigger sent (byte '1') to Arduino");
            } else {
                RCLCPP_ERROR(this->get_logger(), "[SERVO] ✗ Failed to send, errno=%d (%s)", 
                             errno, strerror(errno));
            }
        } else {
            RCLCPP_WARN(this->get_logger(), "[SERVO] Unknown command: '%s'", msg->data.c_str());
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
                
                // Remove carriage return
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                
                // Skip empty lines
                if (line.empty()) {
                    continue;
                }
                
             // UART cat command functionality in logger, for bit tx debug
                RCLCPP_INFO(this->get_logger(), "[DATA] %s", line.c_str());
               
                
                // Handle lines that might start with a comma, uart freaking out @ boot lol
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
                
                // Pad with zeros if needed
                while (distances.size() < 4) {
                    distances.push_back(0);
                }
                
                // Publish to ROS topics
                for (size_t i = 0; i < 4 && i < distances.size(); i++) {
                    auto message = std_msgs::msg::Int32();
                    message.data = distances[i];
                    publishers_[i]->publish(message);
                }
            }
        }
    }

    std::vector<rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr> publishers_;
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