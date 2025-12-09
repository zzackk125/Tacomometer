/*
 * Tacomometer - LVGL Version
 * Target: Waveshare ESP32-C6 Touch AMOLED 1.43
 * Author: zzackk125
 * License: MIT
 */

#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include "src/lvgl_port.h"
#include "src/board_config.h"
#include "src/touch_driver.h"
#include "src/imu_driver.h"
#include "src/ui.h"
#include "src/web_server.h"

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Starting Tacomometer (LVGL)...");

    // 1. Init I2C for IO Expander, Touch, and IMU
    Wire.begin(ESP32_SDA_NUM, ESP32_SCL_NUM);
    delay(100);

    // 2. Enable Display Power (TCA9554 IO Expander)
    Serial.println("Powering up display...");
    
    // Set Pin 6 to Output (0 in Config Register)
    Wire.beginTransmission(IO_EXPANDER_ADDR);
    Wire.write(IO_EXPANDER_CONFIG_REG); 
    Wire.write(~IO_EXPANDER_PIN_6_MASK); // 0xBF: Pin 6 Low (Output), others High (Input) default
    Wire.endTransmission();
    
    // Set Pin 6 High (Power On)
    Wire.beginTransmission(IO_EXPANDER_ADDR);
    Wire.write(IO_EXPANDER_OUTPUT_REG); 
    Wire.write(IO_EXPANDER_PIN_6_MASK); 
    Wire.endTransmission();
    
    delay(200);

    // 3. Init LVGL Port (Display)
    Serial.println("Initializing LVGL Port...");
    lvgl_port_init();
    Serial.println("LVGL Initialized.");

    // 4. Init Touch
    Serial.println("Initializing Touch...");
    initTouch();

    // 5. Init IMU
    Serial.println("Initializing IMU...");
    initIMU();

    // 6. Init UI
    Serial.println("Initializing UI...");
    lvgl_port_lock(-1);
    initUI();
    lvgl_port_unlock();
    
    // 7. Init Web Server (Standby)
    initWebServer();
    
    // 8. Init Button
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
}

// Button State
bool btn_last_state = HIGH;
uint32_t btn_press_start = 0;
bool btn_long_press_handled = false;

void loop() {
    // Read IMU
    updateIMU();
    float roll = getRoll();
    float pitch = getPitch();

    // Update UI (Thread Safe)
    lvgl_port_lock(-1);
    updateUI(roll, pitch);
    
    // Check for calibration trigger (IMU flat detection or button)
    if (isCalibrating()) {
        zeroIMU(); 
    }
    
    lvgl_port_unlock();
    
    // Handle Web Server Clients
    handleWebServer();
    
    // --- Button Logic (BOOT Button) ---
    bool btn_state = digitalRead(BOOT_BUTTON_PIN);
    
    // Press Detected (Active Low)
    if (btn_state == LOW && btn_last_state == HIGH) {
        btn_press_start = millis();
        btn_long_press_handled = false;
    }
    
    // Pending active press
    if (btn_state == LOW) {
        // Check for long press (e.g., 2000ms) to ENABLE WiFi
        if (!isAPMode()) {
             if (millis() - btn_press_start > 2000 && !btn_long_press_handled) {
                // Long Press Detected -> Turn ON AP
                btn_long_press_handled = true;
                lvgl_port_lock(-1);
                showToast("Ready to Pair\nWiFi: Tacomometer\nIP: 192.168.4.1");
                lvgl_port_unlock();
                startAPMode();
             }
        }
    }
    
    // Release Detected
    if (btn_state == HIGH && btn_last_state == LOW) {
        // Logic for SHORT press?
        // Requirement: "If wifi is enabled and the boot button is pressed [implied short], turn off wifi"
        // Also: "When button pressed and held... do same as 5 tap [Enable]"
        
        if (isAPMode()) {
            // If we are already in AP mode, ANY press turns it off? 
            // "If wifi is enabled and the boot button is pressed, turn off wifi"
            // Let's treat a short click as the OFF trigger.
            // Or even a long press? "remove the 5 tap trigger".
            // Let's say < 1000ms is a click.
            // Actually, safest is: If AP is ON, and we release button, turn it OFF if it wasn't a long press?
            // User: "If wifi is enabled and the boot button is pressed, turn off wifi mode and clear the toast message."
            // This implies a simple interaction. I will use Release event.
            // If AP ON -> Turn OFF.
            
            // To prevent accidental turn off immediately after turn on (if user holds too long?), 
            // we should ensure it's a distinct event. 
            // If btn_long_press_handled is true, we just turned it ON, so don't turn it OFF immediately.
            if (!btn_long_press_handled) {
                 stopAPMode();
            }
        }
    }
    
    btn_last_state = btn_state;

    delay(20); // 50Hz Update Rate (Faster for button response)
}
