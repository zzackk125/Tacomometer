import RPi.GPIO as GPIO
import time

BUTTON_PIN = 4

def main():
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(BUTTON_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)
    
    print(f"Testing Button on GPIO {BUTTON_PIN} (Physical Pin 7)")
    print("Press Ctrl+C to exit.")
    print("State: ", "HIGH (Released)" if GPIO.input(BUTTON_PIN) else "LOW (Pressed)")
    
    last_state = GPIO.input(BUTTON_PIN)
    
    try:
        while True:
            current_state = GPIO.input(BUTTON_PIN)
            if current_state != last_state:
                if current_state == GPIO.LOW:
                    print("Button PRESSED!")
                else:
                    print("Button RELEASED!")
                last_state = current_state
            time.sleep(0.05)
            
    except KeyboardInterrupt:
        print("\nExiting...")
    finally:
        GPIO.cleanup()

if __name__ == "__main__":
    main()
