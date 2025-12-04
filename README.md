# Raspberry Pi Inclinometer

This project drives a 1.28inch GC9A01 SPI LCD display on a Raspberry Pi Zero to create a truck inclinometer.

## Hardware Setup

### Wiring

#### Display Connection (GC9A01)

Connect the display to the Raspberry Pi Zero GPIO header as follows:

| Wire Color | Display Pin | Function | Pi Pin | Pi GPIO |
| :--- | :--- | :--- | :--- | :--- |
| **Black** | GND | Ground | Pin 6 | GND |
| **Red** | VCC | Power (3.3V/5V) | Pin 2 | 5V |
| **White** | SCL | SPI Clock | Pin 23 | GPIO 11 (SCLK) |
| **Yellow** | SDA | SPI Data (MOSI) | Pin 19 | GPIO 10 (MOSI) |
| **Orange** | RES | Reset | Pin 13 | GPIO 27 |
| **Green** | DC | Data/Command | Pin 22 | GPIO 25 |
| **Blue** | CS | Chip Select | Pin 24 | GPIO 8 (CE0) |
| **Purple** | BLK | Backlight | Pin 12 | GPIO 18 (PWM) |

#### Raspberry Pi Zero GPIO Pinout

```text
      Raspberry Pi Zero GPIO Header
      -----------------------------
            3.3V [ 1] [ 2] 5V      <-- Red (VIN)
       SDA (I2C) [ 3] [ 4] 5V
       SCL (I2C) [ 5] [ 6] GND     <-- Black (GND)
           GPIO4 [ 7] [ 8] TXD
             GND [ 9] [10] RXD
          GPIO17 [11] [12] GPIO18  <-- Purple (BLK)
Orange (RES) --> [13] [14] GND
          GPIO22 [15] [16] GPIO23
            3.3V [17] [18] GPIO24
Yellow (SDA) --> [19] [20] GND
      MISO (SPI) [21] [22] GPIO25  <-- Green (DC)
 White (SCL) --> [23] [24] CE0     <-- Blue (CS)
             GND [25] [26] CE1
          EEPROM [27] [28] EEPROM
           GPIO5 [29] [30] GND
           GPIO6 [31] [32] GPIO12
          GPIO13 [33] [34] GND
          GPIO19 [35] [36] GPIO16
          GPIO26 [37] [38] GPIO20
             GND [39] [40] GPIO21
```

## Software Setup

1.  **Enable SPI Interface**:
    Run `sudo raspi-config`, navigate to **Interface Options** -> **SPI** and enable it.

2.  **Clone the Repository**:
    If you haven't already, clone this repository to your Raspberry Pi:
    ```bash
    git clone https://github.com/zzackk125/Tacomometer.git
    cd Tacomometer
    ```

2.  **Install Dependencies**:
    It is recommended to use a virtual environment to avoid conflicts with system packages.
    ```bash
    python -m venv venv
    source venv/bin/activate
    pip install -r requirements.txt
    ```
    *Note: If you prefer to install system-wide (not recommended), use `sudo apt install python3-spidev python3-rpi.gpio python3-pil python3-numpy` or pass `--break-system-packages` to pip.*

3.  **Run the Demo**:
    ```bash
    # Make sure your virtual environment is active
    python main.py
    ```


