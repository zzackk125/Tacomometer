# Tacomometer

**The Tacoma-Themed Digital Inclinometer**

A high-performance digital inclinometer for off-road enthusiasts, designed with the aesthetic of the 2nd Gen Toyota Tacoma. Built for the **Waveshare ESP32-C6-Touch-AMOLED-1.43** circular display, it features smooth 60FPS animations, accurate pitch and roll tracking, and customizable alerts.

![Tacomometer Demo](assets/demo.gif) *(Add a demo gif/image here if available)*

## Features

- **Retro-Modern Design:** Inspired by the 2nd Gen Tacoma instrument cluster.
- **High Performance:** Leveraging LVGL v9 for buttery smooth gauge movement (20Hz+ update rate).
- **Dual Axis:** Real-time Pitch and Roll monitoring.
- **Max Angle Memory:** Tracks and displays the maximum angles reached during a session.
- **Dynamic Alerts:**
  - **Warning (Yellow Pulse):** > 30°
  - **Critical (Red Flashing):** > 50°
- **Touch Calibration:** Long-press the screen to zero the inclinometer on a flat surface.

## Hardware Requirements

- **Microcontroller & Display:** [Waveshare ESP32-C6-Touch-AMOLED-1.43](https://www.waveshare.com/wiki/ESP32-C6-Touch-AMOLED-1.43)
- **IMU Sensor:** QMI8658 (or similar 6-axis IMU supported by the driver).
- **Power:** 5V USB-C or 3.7V LiPo Battery.

## Wiring

The default configuration uses the onboard I2C pins for the external IMU.

| Sensor Pin | ESP32 Pin | Description |
| :--- | :--- | :--- |
| **VCC** | 3.3V | Power |
| **GND** | GND | Ground |
| **SDA** | GPIO 18 | I2C Data |
| **SCL** | GPIO 8 | I2C Clock |

## Installation

### Prerequisites

1. **VS Code** with **PlatformIO** (Recommended) OR **Arduino IDE**.
2. **Drivers:** Ensure you have the USB-Serial drivers for the ESP32-C6.

### Arduino IDE Setup

1. Install the **ESP32** board package (v3.0.0+ for ESP32-C6 support) in Board Manager.
2. Select Board: `ESP32C6 Dev Module`.
3. Enable "USB CDC On Boot".
4. Install Required Libraries:
   - `lvgl` (v9.x)
   - `Arduino_GFX_Library` (if used for low-level bus)
   - `SensorQMI8658` (or your specific IMU library)
5. Open `Tacomometer.ino` and upload.

### Configuration

All pin definitions and hardware settings are located in `src/board_config.h`.

## Usage

1. **Mounting:** Mount the device securely in your vehicle.
2. **Power On:** Connect to USB or battery.
3. **Calibration:**
   - Park on a flat, level surface.
   - **Long Press** anywhere on the screen for 2 seconds.
   - The status text will show "CALIBRATING..." and the gauges will reset to 0°.
4. **Drive:** The max indicators (red dots) will track your most extreme angles.

## Customization

### Customization

#### Assets
The project uses python scripts to generate C arrays from images.
1. Place your images in `assets/`.
2. Run `python scripts/generate_assets.py`.
3. Recompile the project.

**Note:** The folder `assets_backup/` contains the default assets (C arrays) for the project. These can be restored if experimental asset generation fails or if you want to revert to the original look without regenerating.

## License

This project is open-source and licensed under the [MIT License](LICENSE).

## Credits
- Built with [LVGL](https://lvgl.io/).
- Developed by **zzackk125**.
