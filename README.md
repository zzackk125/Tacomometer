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

#### MPU-6050 Sensor Connection

Connect the MPU-6050 accelerometer/gyroscope as follows:

| Wire Color | Sensor Pin | Function | Pi Pin | Pi GPIO |
| :--- | :--- | :--- | :--- | :--- |
| **Red** | VCC | Power (5V) | Pin 4 | 5V |
| **Black** | GND | Ground | Pin 9 | GND |
| **Brown** | SCL | I2C Clock | Pin 5 | GPIO 3 (SCL) |
| **White** | SDA | I2C Data | Pin 3 | GPIO 2 (SDA) |
| **Blue** | AD0 | Address Select | Pin 39 | GND |
| **Orange** | XDA | Aux Data | - | Disconnected |
| **Yellow** | XCL | Aux Clock | - | Disconnected |
| **Green** | INT | Interrupt | - | Disconnected |

#### Calibration Button

Connect a momentary push button for zeroing the inclinometer:

| Connection | Pi Pin | Pi GPIO |
| :--- | :--- | :--- |
| Signal | Pin 7 | GPIO 4 |
| Ground | Pin 14 | GND |

#### Raspberry Pi Zero GPIO Pinout

```text
      Raspberry Pi Zero GPIO Header
      -----------------------------
            3.3V  [ 1] [ 2] 5V      <-- Red (Display VIN)
White (SNRSDA)--> [ 3] [ 4] 5V      <-- Red (Sensor VCC)
Brown (SNRSCL)--> [ 5] [ 6] GND     <-- Black (Display GND)
GPIO4 (CalButt)-->[ 7] [ 8] TXD     
Black (SNRGND)--> [ 9] [10] RXD
          GPIO17  [11] [12] GPIO18  <-- Purple (Display BLK)
Orange (DSPRES)-->[13] [14] GND     
          GPIO22  [15] [16] GPIO23
            3.3V  [17] [18] GPIO24
Yellow (DSPSDA)-->[19] [20] GND
             MISO [21] [22] GPIO25  <-- Green (Display DC)
White (DSPSCL)--> [23] [24] CE0     <-- Blue (Display CS)
             GND  [25] [26] CE1
           EEPROM [27] [28] EEPROM
            GPIO5 [29] [30] GND
            GPIO6 [31] [32] GPIO12
           GPIO13 [33] [34] GND     <-- Calibration Button GND
           GPIO19 [35] [36] GPIO16
           GPIO26 [37] [38] GPIO20
              GND [39] [40] GPIO21
```

## Software Setup

1.  **Enable SPI Interface**:
    Run `sudo raspi-config`, navigate to **Interface Options** -> **SPI** and enable it.
    Also navigate to **Interface Options** -> **I2C** and enable it.

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

## Run on Boot
To have the inclinometer start automatically when the Raspberry Pi turns on, you can create a systemd service.

1.  **Create the Service File**:
    ```bash
    sudo nano /etc/systemd/system/tacomometer.service
    ```

2.  **Paste the Following Configuration**:
    *Adjust the paths (`/home/pi/Tacomometer`) if you cloned it somewhere else.*
    ```ini
    [Unit]
    Description=Tacomometer Inclinometer Service
    After=multi-user.target

    [Service]
    Type=simple
    User=pi
    WorkingDirectory=/home/pi/Tacomometer
    ExecStart=/home/pi/Tacomometer/venv/bin/python /home/pi/Tacomometer/main.py
    Restart=on-failure
    RestartSec=5
    StandardOutput=journal
    StandardError=journal

    [Install]
    WantedBy=multi-user.target
    ```

3.  **Enable and Start the Service**:
    ```bash
    sudo systemctl daemon-reload
    sudo systemctl enable tacomometer.service
    sudo systemctl start tacomometer.service
    ```

4.  **Check Status**:
    ```bash
    sudo systemctl status tacomometer.service
    ```


