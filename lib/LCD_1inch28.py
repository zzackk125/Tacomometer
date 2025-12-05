
import time
import spidev
import RPi.GPIO as GPIO
import logging

# Pin definitions
RST_PIN = 27
DC_PIN = 25
BL_PIN = 18
CS_PIN = 8

class LCD_1inch28(object):
    def __init__(self):
        self.width = 240
        self.height = 240
        
        # Initialize GPIO
        GPIO.setmode(GPIO.BCM)
        GPIO.setwarnings(False)
        GPIO.setup(RST_PIN, GPIO.OUT)
        GPIO.setup(DC_PIN, GPIO.OUT)
        GPIO.setup(BL_PIN, GPIO.OUT)
        
        # Initialize SPI
        self.spi = spidev.SpiDev(0, 0)
        self.spi.max_speed_hz = 60000000
        self.spi.mode = 0b00
        
        self.init()

    def reset(self):
        """Hardware reset"""
        GPIO.output(RST_PIN, GPIO.HIGH)
        time.sleep(0.01)
        GPIO.output(RST_PIN, GPIO.LOW)
        time.sleep(0.01)
        GPIO.output(RST_PIN, GPIO.HIGH)
        time.sleep(0.01)

    def send_command(self, cmd):
        """Send command via SPI"""
        GPIO.output(DC_PIN, GPIO.LOW)
        self.spi.writebytes([cmd])

    def send_data(self, data):
        """Send data via SPI"""
        GPIO.output(DC_PIN, GPIO.HIGH)
        self.spi.writebytes([data])
        
    def send_data_list(self, data_list):
        """Send list of data via SPI"""
        GPIO.output(DC_PIN, GPIO.HIGH)
        # Split into chunks of 4096 bytes to avoid SPI buffer limits
        chunk_size = 4096
        for i in range(0, len(data_list), chunk_size):
            self.spi.writebytes(data_list[i:i+chunk_size])

    def init(self):
        """Initialize the GC9A01 display"""
        self.reset()
        
        self.send_command(0xEF)
        
        self.send_command(0xEB)
        self.send_data(0x14) 
        
        self.send_command(0xFE) 
        self.send_command(0xEF) 

        self.send_command(0xEB) 
        self.send_data(0x14) 

        self.send_command(0x84) 
        self.send_data(0x40) 

        self.send_command(0x85) 
        self.send_data(0xFF) 

        self.send_command(0x86) 
        self.send_data(0xFF) 

        self.send_command(0x87) 
        self.send_data(0xFF) 

        self.send_command(0x88) 
        self.send_data(0x0A) 

        self.send_command(0x89) 
        self.send_data(0x21) 

        self.send_command(0x8A) 
        self.send_data(0x00) 

        self.send_command(0x8B) 
        self.send_data(0x80) 

        self.send_command(0x8C) 
        self.send_data(0x01) 

        self.send_command(0x8D) 
        self.send_data(0x01) 

        self.send_command(0x8E) 
        self.send_data(0xFF) 

        self.send_command(0x8F) 
        self.send_data(0xFF) 

        self.send_command(0xB6) 
        self.send_data(0x00) 
        self.send_data(0x20) 

        self.send_command(0x36) 
        self.send_data(0x08) # BGR color

        self.send_command(0x3A) 
        self.send_data(0x05) 

        self.send_command(0x90) 
        self.send_data(0x08) 
        self.send_data(0x08) 
        self.send_data(0x08) 
        self.send_data(0x08) 

        self.send_command(0xBD) 
        self.send_data(0x06) 

        self.send_command(0xBC) 
        self.send_data(0x00) 

        self.send_command(0xFF) 
        self.send_data(0x60) 
        self.send_data(0x01) 
        self.send_data(0x04) 

        self.send_command(0xC3) 
        self.send_data(0x13) 
        self.send_command(0xC4) 
        self.send_data(0x13) 

        self.send_command(0xC9) 
        self.send_data(0x22) 

        self.send_command(0xBE) 
        self.send_data(0x11) 

        self.send_command(0xE1) 
        self.send_data(0x10) 
        self.send_data(0x0E) 

        self.send_command(0xDF) 
        self.send_data(0x21) 
        self.send_data(0x0c) 
        self.send_data(0x02) 

        self.send_command(0xF0) 
        self.send_data(0x45) 
        self.send_data(0x09) 
        self.send_data(0x08) 
        self.send_data(0x08) 
        self.send_data(0x26) 
        self.send_data(0x2A) 

        self.send_command(0xF1) 
        self.send_data(0x43) 
        self.send_data(0x70) 
        self.send_data(0x72) 
        self.send_data(0x36) 
        self.send_data(0x37) 
        self.send_data(0x6F) 

        self.send_command(0xF2) 
        self.send_data(0x45) 
        self.send_data(0x09) 
        self.send_data(0x08) 
        self.send_data(0x08) 
        self.send_data(0x26) 
        self.send_data(0x2A) 

        self.send_command(0xF3) 
        self.send_data(0x43) 
        self.send_data(0x70) 
        self.send_data(0x72) 
        self.send_data(0x36) 
        self.send_data(0x37) 
        self.send_data(0x6F) 

        self.send_command(0xED) 
        self.send_data(0x1B) 
        self.send_data(0x0B) 

        self.send_command(0xAE) 
        self.send_data(0x77) 

        self.send_command(0xCD) 
        self.send_data(0x63) 

        self.send_command(0x70) 
        self.send_data(0x07) 
        self.send_data(0x07) 
        self.send_data(0x04) 
        self.send_data(0x0E) 
        self.send_data(0x0F) 
        self.send_data(0x09) 
        self.send_data(0x07) 
        self.send_data(0x08) 
        self.send_data(0x03) 

        self.send_command(0xE8) 
        self.send_data(0x34) 

        self.send_command(0x62) 
        self.send_data(0x18) 
        self.send_data(0x0D) 
        self.send_data(0x71) 
        self.send_data(0xED) 
        self.send_data(0x70) 
        self.send_data(0x70) 
        self.send_data(0x18) 
        self.send_data(0x0F) 
        self.send_data(0x71) 
        self.send_data(0xEF) 
        self.send_data(0x70) 
        self.send_data(0x70) 

        self.send_command(0x63) 
        self.send_data(0x18) 
        self.send_data(0x11) 
        self.send_data(0x71) 
        self.send_data(0xF1) 
        self.send_data(0x70) 
        self.send_data(0x70) 
        self.send_data(0x18) 
        self.send_data(0x13) 
        self.send_data(0x71) 
        self.send_data(0xF3) 
        self.send_data(0x70) 
        self.send_data(0x70) 

        self.send_command(0x64) 
        self.send_data(0x28) 
        self.send_data(0x29) 
        self.send_data(0xF1) 
        self.send_data(0x01) 
        self.send_data(0xF1) 
        self.send_data(0x00) 
        self.send_data(0x07) 

        self.send_command(0x66) 
        self.send_data(0x3C) 
        self.send_data(0x00) 
        self.send_data(0xCD) 
        self.send_data(0x67) 
        self.send_data(0x45) 
        self.send_data(0x45) 
        self.send_data(0x10) 
        self.send_data(0x00) 
        self.send_data(0x00) 
        self.send_data(0x00) 

        self.send_command(0x67) 
        self.send_data(0x00) 
        self.send_data(0x3C) 
        self.send_data(0x00) 
        self.send_data(0x00) 
        self.send_data(0x00) 
        self.send_data(0x01) 
        self.send_data(0x54) 
        self.send_data(0x10) 
        self.send_data(0x32) 
        self.send_data(0x98) 

        self.send_command(0x74) 
        self.send_data(0x10) 
        self.send_data(0x85) 
        self.send_data(0x80) 
        self.send_data(0x00) 
        self.send_data(0x00) 
        self.send_data(0x4E) 
        self.send_data(0x00) 

        self.send_command(0x98) 
        self.send_data(0x3e) 
        self.send_data(0x07) 

        self.send_command(0x35) 
        self.send_command(0x21) 

        self.send_command(0x11) 
        time.sleep(0.12) 
        self.send_command(0x29) 
        time.sleep(0.02)
        
        # Turn on backlight
        GPIO.output(BL_PIN, GPIO.HIGH)

    def SetWindows(self, Xstart, Ystart, Xend, Yend):
        self.send_command(0x2A)
        self.send_data(0x00)
        self.send_data(Xstart & 0xFF)
        self.send_data(0x00)
        self.send_data((Xend - 1) & 0xFF)

        self.send_command(0x2B)
        self.send_data(0x00)
        self.send_data(Ystart & 0xFF)
        self.send_data(0x00)
        self.send_data((Yend - 1) & 0xFF)

        self.send_command(0x2C)

    def ShowImage(self, Image):
        """Send PIL Image to display"""
        imwidth, imheight = Image.size
        if imwidth != self.width or imheight != self.height:
            raise ValueError('Image must be same dimensions as display \
                ({0}x{1}).' .format(self.width, self.height))
        
        # Convert to RGB to ensure we have R,G,B channels
        img = Image.convert('RGB')
        pixels = list(img.getdata())
        
        buffer = []
        for r, g, b in pixels:
            # Convert RGB888 to RGB565
            # High byte: RRRRRGGG (R: 5 bits, G: 3 bits)
            # Low byte:  GGGBBBBB (G: 3 bits, B: 5 bits)
            high = (r & 0xF8) | (g >> 5)
            low = ((g & 0x1C) << 3) | (b >> 3)
            buffer.append(high)
            buffer.append(low)
            
        self.SetWindows(0, 0, self.width, self.height)
        self.send_data_list(buffer)
        
    def cleanup(self):
        GPIO.cleanup()
        self.spi.close()
