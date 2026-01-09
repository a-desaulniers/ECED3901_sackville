# Written by Alexandre DesAulniers
# Written January 9th 2025
# Install Script for ROS 2 Jazzy & Gazebo Harmonic (Ubuntu 24.04)

set -e # Exit immediately if a command fails

echo "--- Setting up Locales ---"
sudo apt update && sudo apt install locales -y
sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
export LANG=en_US.UTF-8

echo "--- Adding ROS 2 Repository ---"
sudo apt install curl gnupg2 lsb-release software-properties-common -y
sudo add-apt-repository universe -y

sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key -o /usr/share/keyrings/ros-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(lsb_release -cs) main" | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null

echo "--- Installing ROS 2 Jazzy & Gazebo Harmonic ---"
sudo apt update
sudo apt install ros-jazzy-desktop ros-jazzy-ros-gz ros-dev-tools python3-rosdep -y

echo "--- Initializing rosdep ---"
if [ ! -f /etc/ros/rosdep/sources.list.d/20-default.list ]; then
    sudo rosdep init
fi
rosdep update

echo "--- Finalizing Environment ---"
# Check if source is already in .bashrc to avoid duplicates
if ! grep -q "source /opt/ros/jazzy/setup.bash" ~/.bashrc; then
    echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
fi

source /opt/ros/jazzy/setup.bash

ros2 --version

echo "Installation complete. Please restart your terminal or run: source ~/.bashrc"
