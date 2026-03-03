// Author: Alexandre DesAulniers
// ROS2 Serial Ultrasonic Driver
// Written Mar 2nd 2026

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

using namespace std::chrono_literals;

class SerialBridgeNode : public rclcpp::Node {
public:
    SerialBridgeNode() : Node("serial_bridge_node") {
        // create 4 publishers, one for each sensor
        publishers_.push_back(this->create_publisher<std_msgs::msg::Int32>("ultrasonic_1", 10));
        publishers_.push_back(this->create_publisher<std_msgs::msg::Int32>("ultrasonic_2", 10));
        publishers_.push_back(this->create_publisher<std_msgs::msg::Int32>("ultrasonic_3", 10));
        publishers_.push_back(this->create_publisher<std_msgs::msg::Int32>("ultrasonic_4", 10));
        
        RCLCPP_INFO(this->get_logger(), "Serial Bridge Node started - reading 4 sensors from /dev/ttyUSB0");
        RCLCPP_INFO(this->get_logger(), "Publishing to: ultrasonic_1, ultrasonic_2, ultrasonic_3, ultrasonic_4");
        
        // Open connection to serial device
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
        
        tty.c_cflag |= (CLOCAL | CREAD);    // ignore modem controls
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;                 //8 bit characters
        tty.c_cflag &= ~PARENB;             //no parity bit
        tty.c_cflag &= ~CSTOPB;              // only need 1 stop bit
        tty.c_cflag &= ~CRTSCTS;             // no hardware flowcontrol
        
        // Set raw input mode (non canonical mode)
        tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        tty.c_iflag &= ~(IXON | IXOFF | IXANY);
        tty.c_iflag &= ~(INLCR | ICRNL | IGNCR);
        tty.c_oflag &= ~OPOST;
        
        // VMIN = 0, VTIME = 1 means read returns immediately with any available data
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 1;
        
        tcsetattr(serial_port_, TCSANOW, &tty);
        
        // Flush any initial shit 
        tcflush(serial_port_, TCIOFLUSH);
        
        // timer loop to read serial
        timer_ = this->create_wall_timer(10ms, std::bind(&SerialBridgeNode::read_serial, this));
    }

    ~SerialBridgeNode() { 
        if (serial_port_ >= 0) {
            close(serial_port_); 
        }
    }

private:
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
            
            RCLCPP_DEBUG(this->get_logger(), "Raw line: '%s'", line.c_str());
            
            // Handle lines that might start with a comma
            if (!line.empty() && line[0] == ',') {
                line = "0" + line;  // Prepend a zero
            }
            
            // Parse CSV
            std::vector<int> distances;
            std::stringstream ss(line);
            std::string token;
            
            while (std::getline(ss, token, ',')) {
                // Handle empty tokens (like ",,")
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
            
            // Pad with zeros if we got fewer than 4 values
            while (distances.size() < 4) {
                distances.push_back(0);
            }
            
            // Publish (take first 4 values)
            for (size_t i = 0; i < 4 && i < distances.size(); i++) {
                auto message = std_msgs::msg::Int32();
                message.data = distances[i];
                publishers_[i]->publish(message);
            }
            
            // Log what we published
            RCLCPP_INFO(this->get_logger(), "Published: %d,%d,%d,%d", 
                distances[0], distances[1], distances[2], distances[3]);
        }
    }
}

    std::vector<rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr> publishers_;
    rclcpp::TimerBase::SharedPtr timer_;
    int serial_port_;
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<SerialBridgeNode>());
    rclcpp::shutdown();
    return 0;
}