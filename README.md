<p align="center">
<img width="250" height="250" alt="image_5" src="https://github.com/user-attachments/assets/fd3b9162-d2ce-45eb-8df0-d2659b4f6e30" />
</p>

<div align="center">
  
## ECED3901: Team III — `_sackville`

</div>

&nbsp;

<div align="center">
  
***"Pro Gredimur"***

</div>

&nbsp;

This repository contains the design, implementation, and analysis for the Team III entry in the **ECED3901 Peter Gregson Design Challenge**. Project `_sackville` focuses on the development of an autonomous robotic vessel capable of navigation, cargo handling, and obstacle detection.

## Team Leads
* **Owen Melanson:** Project Lead
* **Michael Doyle:** Navigations Lead
* **Alexandre DesAulniers:** System Integration & Power Systems Lead
* **Assadah Kausar:** Auxiliary Lead
* **Andrew Franklin:** Design Lead

## Project Scope & Subsystems
The project is divided into several core subsystems designed to meet specific challenge requirements:

### 1. Navigation & Control
* ROS2-based waypoint navigation with SLAM.
* State-machine controller utilizing a UART bridge driver for subsystem actuation.
* Achieved a mean distance error of **8.67 cm** and a mean angular error of **8.00°** during testing.

### 2. Power Systems
* **Design:** A regulated 5V Buck-Boost Converter based on Ni-MH AA cells.
* **Efficiency:** Maintained high transfer efficiency (up to 98.24% at 500mA) across the operating range.

### 3. Cargo Systems
* **Acquisition:** Polyethylene adhesive system on the front bumper for cargo pickup.
* **Delivery:** Servo-controlled ramp with a corrugated fiberboard restrainment system.

### 4. Auxiliary & Safety (COLREG)
* **Threat Detection:** Quad Ultrasonic detection using an ATMega328P-based MCU interface.
* **Signal Stabilization:** Filtered sensor readings to ensure stable COLREG signals despite environmental noise.
* **Communication:** UART-enabled real-time monitoring of sensors and lighting systems.

## Results Summary
| Subsystem | Status | Note |
| :--- | :--- | :--- |
| **Navigation** | ✅ Success | Completed within reduced scope |
| **Lights** | ✅ Success | Successfully integrated |
| **Tape/Pickup** | ✅ Success | Fully operational |
| **Cargo Ramp** | ❌ Failure | Attributed to a shorted ultrasonic extension cable |
| **FSK Module** | ⚪ Unused | Not implemented in final run |

## Lessons Learned
* **Hardware Integrity:** Ensure clean wiring to allow for rapid error detection.
* **Resource Management:** Better allocation of team member strengths is critical for complex integration.
* **Environmental Testing:** Always validate machine performance in an environment that matches the final application parameters.

## Tech Stack & Skills
* **Software:** ROS2, UNIX, FOSS 3D Modeling
* **Hardware:** Altium (PCB Design), Power Supply Modeling (LTspice/TINA), ATMega328P
