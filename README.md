# Raspberry Pi Inclinometer

This project drives a 1.28inch GC9A01 SPI LCD display on a Raspberry Pi Zero to create a truck inclinometer.

## Hardware Setup

### Wiring (BCM Pinout)

| Display Pin | Raspberry Pi Pin | BCM GPIO | Function |
| :--- | :--- | :--- | :--- |
| VCC | 3.3V | - | Power |
| GND | GND | - | Ground |
| DIN | MOSI | 10 | SPI Data Input |
| CLK | SCLK | 11 | SPI Clock |
| CS | CE0 | 8 | Chip Select |
| DC | GPIO 25 | 25 | Data/Command |
| RST | GPIO 27 | 27 | Reset |
| BL | GPIO 18 | 18 | Backlight |

## Software Setup

1.  **Enable SPI Interface**:
    Run `sudo raspi-config`, navigate to **Interface Options** -> **SPI** and enable it.

2.  **Install Dependencies**:
    ```bash
    pip install -r requirements.txt
    ```

3.  **Run the Demo**:
    ```bash
    python main.py
    ```

4.  **Clone the Repository**:
    If you haven't already, clone this repository to your Raspberry Pi:
    ```bash
    git clone https://github.com/YOUR_USERNAME/Tacomometer.git
    cd Tacomometer
    ```
