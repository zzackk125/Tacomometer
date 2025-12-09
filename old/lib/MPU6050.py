import smbus2
import math
import time

# MPU6050 Registers
MPU6050_ADDR = 0x68
PWR_MGMT_1   = 0x6B
ACCEL_XOUT_H = 0x3B

class MPU6050:
    def __init__(self, bus_num=1):
        self.bus = smbus2.SMBus(bus_num)
        self.address = MPU6050_ADDR
        self.init_sensor()

    def init_sensor(self):
        # Wake up the sensor (clear sleep bit)
        self.bus.write_byte_data(self.address, PWR_MGMT_1, 0)

    def read_raw_data(self, addr):
        # Read two bytes (high and low)
        high = self.bus.read_byte_data(self.address, addr)
        low = self.bus.read_byte_data(self.address, addr + 1)
        
        # Combine them
        value = (high << 8) | low
        
        # Handle signed 16-bit integer
        if value > 32768:
            value = value - 65536
        return value

    def get_accel_data(self):
        x = self.read_raw_data(ACCEL_XOUT_H)
        y = self.read_raw_data(ACCEL_XOUT_H + 2)
        z = self.read_raw_data(ACCEL_XOUT_H + 4)
        
        # Scale factor for +/- 2g is 16384.0 LSB/g
        # We don't strictly need to convert to g for atan2, but it's good practice
        scale = 16384.0
        return x/scale, y/scale, z/scale

    def get_angles(self):
        ax, ay, az = self.get_accel_data()
        
        # Calculate Pitch and Roll (in degrees)
        # Roll: Rotation around X-axis (tilting left/right)
        # Pitch: Rotation around Y-axis (tilting forward/back)
        
        # Using standard aerodynamic formulas:
        # Roll = atan2(Y, Z)
        # Pitch = atan2(-X, sqrt(Y*Y + Z*Z))
        
        roll = math.degrees(math.atan2(ay, az))
        pitch = math.degrees(math.atan2(-ax, math.sqrt(ay*ay + az*az)))
        
        return roll, pitch
