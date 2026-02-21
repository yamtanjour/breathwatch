## BreatheWatch

Air quality monitor built on the STM32F401.

## What it does
- Reads temperature, humidity (AHT21) and air quality / eCO2 / TVOC (ENS160) over I2C
- Logs sensor data to internal flash
- Sends data over UART to a companion Flutter app via Bluetooth

## Hardware used
- STM32F401 microcontroller
- AHT21 temperature & humidity sensor
- ENS160 air quality sensor
- Custom PCB (see `hardware (PCB)/`) (for later)

