// Author: Alexandre DesAulniers
// Last Updated, Feb 11th 2026 @ 11:08AM

#include <chrono>
#include <functional>
#include <memory>
#include <string>
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
        publisher_ = this->create_publisher<std_msgs::msg::Int32>("ultrasonic_distance", 10);
        
        // open connection to linux serial device, should be ttyUSB0 if only one UART bus is activated.
        // for multiple UART nodes, we'll publish to multiple topics. 
        
        serial_port_ = open("/dev/ttyUSB0", O_RDWR);

        // set 115200 baud for Serial Port (only partially understand this syntax. i know what's happening here at least -AD)
        struct termios tty;
        if(tcgetattr(serial_port_, &tty) != 0) {
            RCLCPP_ERROR(this->get_logger(), "Error from tcgetattr: %s", strerror(errno));
        }

        cfsetospeed(&tty, B115200);
        cfsetispeed(&tty, B115200);
        tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;                 /* 8-bit characters */
        tty.c_cflag &= ~PARENB;             /* no parity bit */
        tty.c_cflag &= ~CSTOPB;             /* only need 1 stop bit */
        tty.c_cflag &= ~CRTSCTS;            /* no hardware flowcontrol */

        tcsetattr(serial_port_, TCSANOW, &tty);

        // Ceate timer loop to read serial
        timer_ = this->create_wall_timer(10ms, std::bind(&SerialBridgeNode::read_serial, this));
    }

    ~SerialBridgeNode() { close(serial_port_); }

private:
    void read_serial() {
        char buf[16];
        int n = read(serial_port_, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            try {
                auto message = std_msgs::msg::Int32();
                message.data = std::stoi(buf);
                publisher_->publish(message);
            } catch (...) {
                // Ignore partial lines or other non integer shit
            }
        }
    }

    rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
    int serial_port_;
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<SerialBridgeNode>());
    rclcpp::shutdown();
    return 0;
}
