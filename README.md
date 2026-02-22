# BreathWatch – Portable Air Quality Monitor

![BreathWatch breadboard prototype](<hardware (PCB)/diagrams-images/prototype - breadboard.jpeg>)
*Prototype using breadboard*

Personal IoT device using STM32 to monitor indoor air quality and send data to a Flutter mobile app via Bluetooth.

## Hardware (PCB Design)

![Custom PCB 3D render](<hardware (PCB)/diagrams-images/pcb-3d.png>)
*3D view of PCB*

![PCB design](<hardware (PCB)/diagrams-images/pcb-design.png>)
*PCB layout*

## Circuit Overview

![Wiring diagram](<hardware (PCB)/diagrams-images/circuit-diagram.png>)
*Detailed connections*

![PCB circuit diagram](<hardware (PCB)/diagrams-images/circuit-diagram-pcb.png>)
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