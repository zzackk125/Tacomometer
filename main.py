import time
import json
import os
import logging
import RPi.GPIO as GPIO
from lib.MPU6050 import MPU6050
from inclinometer import InclinometerUI

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Constants
CALIBRATION_FILE = "calibration.json"
BUTTON_PIN = 4

def load_calibration():
    if os.path.exists(CALIBRATION_FILE):
        try:
            with open(CALIBRATION_FILE, 'r') as f:
                return json.load(f)
        except Exception as e:
            logger.error(f"Error loading calibration: {e}")
    return {"roll_offset": 0.0, "pitch_offset": 0.0}

def save_calibration(offsets):
    try:
        with open(CALIBRATION_FILE, 'w') as f:
            json.dump(offsets, f)
        logger.info("Calibration saved.")
    except Exception as e:
        logger.error(f"Error saving calibration: {e}")

def main():
    # Setup GPIO for Calibration Button
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(BUTTON_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)

    try:
        logger.info("Initializing Sensor...")
        sensor = MPU6050()
        
        logger.info("Initializing UI...")
        ui = InclinometerUI()
        
        # Load offsets
        offsets = load_calibration()
        roll_offset = offsets.get("roll_offset", 0.0)
        pitch_offset = offsets.get("pitch_offset", 0.0)
        
        logger.info("Starting Main Loop...")
        
        while True:
            # Read Sensor
            try:
                raw_roll, raw_pitch = sensor.get_angles()
            except Exception as e:
                logger.error(f"Sensor Error: {e}")
                continue

            # Check Calibration Button (Active Low)
            if GPIO.input(BUTTON_PIN) == GPIO.LOW:
                logger.info("Calibration Button Pressed!")
                # Debounce
                time.sleep(0.2)
                # Average a few readings for better calibration
                cal_r, cal_p = 0, 0
                for _ in range(10):
                    r, p = sensor.get_angles()
                    cal_r += r
                    cal_p += p
                    time.sleep(0.01)
                
                roll_offset = cal_r / 10.0
                pitch_offset = cal_p / 10.0
                
                save_calibration({"roll_offset": roll_offset, "pitch_offset": pitch_offset})
                logger.info(f"New Offsets -> Roll: {roll_offset:.2f}, Pitch: {pitch_offset:.2f}")
                
                # Wait for button release
                while GPIO.input(BUTTON_PIN) == GPIO.LOW:
                    time.sleep(0.1)

            # Apply Calibration
            # We want the displayed value to be 0 when at the calibrated position.
            # Display = Raw - Offset
            curr_roll = raw_roll - roll_offset
            curr_pitch = raw_pitch - pitch_offset
            
            # Update UI
            ui.update(curr_roll, curr_pitch)
            
            # Limit frame rate (approx 30 FPS)
            # time.sleep(0.033) 
            # Actually, the UI update (drawing + SPI) takes time, so we might not need explicit sleep.
            # But a small sleep yields CPU.
            time.sleep(0.01)

    except KeyboardInterrupt:
        logger.info("Exiting...")
    except Exception as e:
        logger.error(f"Critical Error: {e}")
    finally:
        GPIO.cleanup()

if __name__ == "__main__":
    main()
