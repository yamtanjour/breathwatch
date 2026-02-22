# BreathWatch – Portable Air Quality Monitor

<img src="hardware (PCB)/diagrams-images/prototype - breadboard.jpeg" width="500" alt="BreathWatch breadboard prototype">

*Prototype using breadboard*

Personal IoT device using STM32 to monitor indoor air quality and send data to a Flutter mobile app via Bluetooth.

## Hardware (PCB Design)

<img src="hardware (PCB)/diagrams-images/pcb-3d.png" width="500" alt="Custom PCB 3D render">

*3D view of PCB*

<img src="hardware (PCB)/diagrams-images/pcb-design.png" width="500" alt="PCB design">

*PCB layout*

## Circuit Overview

<img src="hardware (PCB)/diagrams-images/circuit-diagram.png" width="500" alt="Wiring diagram">

*Detailed connections*

<img src="hardware (PCB)/diagrams-images/circuit-diagram-pcb.png" width="500" alt="PCB circuit diagram">

*PCB schematic*

## Features

- **Sensors:** ENS160 (eCO2 / TVOC / air quality index) + AHT21 (temperature & humidity) over I2C
- **Bluetooth UART bridge** to Flutter companion app
- **Push button & status LED** control via MOSFET

## Hardware
- STM32F401 microcontroller dev board
- ENS160 air quality sensor
- AHT21 temperature & humidity sensor
- PCB 