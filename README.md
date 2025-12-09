# Tacomometer (ESP32-C6 Edition)

A digital inclinometer for off-road vehicles, ported to the **Waveshare ESP32-C6 Touch AMOLED 1.43**.

## Hardware
- **Board**: Waveshare ESP32-C6 Touch AMOLED 1.43
- **Display**: 1.43" AMOLED (466x466)
- **IMU**: QMI8658 (6-axis Accelerometer/Gyro)
- **Touch**: FT6146/FT6236 Capacitive Touch

## Features
- **Real-time Roll & Pitch**: Smooth visualization of vehicle angle.
- **High-Res Graphics**: Custom rendered truck sprites (Rear & Side views).
- **Calibration**: Touch and hold screen for 2 seconds to zero the gauges.
- **Critical Alerts**: Visual warning when angles exceed safe limits (>45°).

## Setup Instructions

### 1. Arduino IDE Setup
1.  Install **Arduino IDE** (2.0+ recommended).
2.  Add ESP32 Board Manager URL:
    `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3.  Install **ESP32** board package (Version 3.0+).
4.  **Tools Menu Configuration**:
    Configure your "Tools" menu exactly as follows:
    - **Board**: `ESP32C6 Dev Module`
    - **USB CDC On Boot**: `Enabled` (Crucial for Serial Monitor)
    - **CPU Frequency**: `160MHz (WiFi)`
    - **Core Debug Level**: `None`
    - **Erase All Flash Before Sketch Upload**: `Disabled`
    - **Flash Frequency**: `80MHz`
    - **Flash Mode**: `QIO`
    - **Flash Size**: `16MB (128Mb)`
    - **JTAG Adapter**: `Disabled`
    - **Partition Scheme**: `16M Flash (3MB APP/9.9MB FATFS)`
    - **Upload Speed**: `921600`
    - **Zigbee Mode**: `Disabled`

### 2. Required Libraries
Install the following via Arduino Library Manager:
- **Arduino_GFX_Library** (by Moononournation) - *Check for latest version supporting CO5300*.
- **Wire** (Built-in).

### 3. Compilation
1.  Open `Tacomometer.ino`.
2.  Connect the board via USB-C.
3.  Select the correct COM port.
4.  Click **Upload**.

## Usage
- **Power On**: The device will boot and show the inclinometer interface.
- **Calibration**: Park on a level surface. Touch and hold anywhere on the screen for **2 seconds** until "CALIBRATED!" appears.
- **Mounting**: Mount the device securely on your dashboard. Ensure the orientation matches the truck graphics (Rear view = Roll, Side view = Pitch).

## Legacy Code
The original Python code for Raspberry Pi Zero is preserved in the `old/` directory for reference.
