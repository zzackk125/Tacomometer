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
        
        # Sensor Data Buffer for Moving Average
        # Reduced to 3 for minimal lag while keeping some noise rejection
        roll_buffer = [0.0] * 3
        pitch_buffer = [0.0] * 3
        
        logger.info("Starting Main Loop...")
        
        while True:
            # Read Sensor
            try:
                raw_roll, raw_pitch = sensor.get_angles()
            except Exception as e:
                logger.error(f"Sensor Error: {e}")
                continue

            # Update buffers
            roll_buffer.pop(0)
            roll_buffer.append(raw_roll)
            pitch_buffer.pop(0)
            pitch_buffer.append(raw_pitch)
            
            # Calculate average
            avg_roll = sum(roll_buffer) / len(roll_buffer)
            avg_pitch = sum(pitch_buffer) / len(pitch_buffer)

            # Check Calibration Button (Active Low)
            if GPIO.input(BUTTON_PIN) == GPIO.LOW:
                logger.info("Button pressed... Hold for 2 seconds to calibrate.")
                start_time = time.time()
                held_long_enough = False
                
                # Wait while button is held
                while GPIO.input(BUTTON_PIN) == GPIO.LOW:
                    if time.time() - start_time > 2.0:
                        held_long_enough = True
                        break
                    time.sleep(0.1)
                
                if held_long_enough:
                    logger.info("Calibrating...")
                    # Wait a tiny bit for any vibration to settle
                    time.sleep(0.5)
                    
                    # Average readings
                    cal_r, cal_p = 0, 0
                    samples = 20
                    for i in range(samples):
                        r, p = sensor.get_angles()
                        cal_r += r
                        cal_p += p
                        
                        # Flash UI
                        flash = (i // 2) % 2 == 0
                        ui.show_calibration_screen(flash)
                        
                        time.sleep(0.05)
                    
                    roll_offset = cal_r / samples
                    pitch_offset = cal_p / samples
                    
                    save_calibration({"roll_offset": roll_offset, "pitch_offset": pitch_offset})
                    logger.info(f"Calibration Complete! Offsets -> Roll: {roll_offset:.2f}, Pitch: {pitch_offset:.2f}")
                    
                    # Wait for release to avoid re-triggering immediately
                    while GPIO.input(BUTTON_PIN) == GPIO.LOW:
                        time.sleep(0.1)
                else:
                    logger.info("Calibration cancelled (button released too early).")

            # Apply Calibration
            # We want the displayed value to be 0 when at the calibrated position.
            # Display = Raw - Offset
            curr_roll = avg_roll - roll_offset
            curr_pitch = avg_pitch - pitch_offset
            
            # Update UI
            ui.update(curr_roll, curr_pitch)
            
            # Limit frame rate (approx 30 FPS)
            # Removed sleep to maximize FPS
            # time.sleep(0.01)

    except KeyboardInterrupt:
        logger.info("Exiting...")
    except Exception as e:
        logger.error(f"Critical Error: {e}")
    finally:
        GPIO.cleanup()

if __name__ == "__main__":
    main()
