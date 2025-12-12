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

    // 0. Check Boot Button for Factory Reset (Safe Mode)
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
        Serial.println("Button Pressed at Boot -> Factory Reset...");
        delay(100); // Debounce
        // We need UI initialized first to reset UI preferences?
        // ui_prefs is in ui.cpp. 
        // We can init NVS manually or just init UI then reset.
        // Let's Init everything then Reset.
    }
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
    
    // 6b. FACTORY RESET CHECK (Dedicated Window)
    // Hold BOOT button during first 2 seconds to reset
    Serial.println("Press BOOT button now to Factory Reset...");
    unsigned long boot_check_start = millis();
    bool reset_triggered = false;
    
    // Initial check + small window
    while(millis() - boot_check_start < 2000) {
        if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
            // Button is pressed
            delay(50); // Debounce
            if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
                 Serial.println("FACTORY RESET TRIGGERED!");
                 showToast("Resetting...");
                 // Wait a moment for toast to render
                 lv_timer_handler(); // Force UI update
                 delay(500); 
                 resetSettings(); // WWill wipe and Restart
                 reset_triggered = true;
                 break;
            }
        }
        delay(10);
    }
    
    if (!reset_triggered) {
         Serial.println("Boot Continues...");
    }
    
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

// Double Tap State
uint32_t last_btn_release = 0;
int btn_click_count = 0;

// --- Background NVS Save (Deferred) ---
bool save_cal_pending = false;
unsigned long save_cal_timer = 0;

void loop() {
    // Read IMU
    updateIMU();
    float roll = getRoll();
    float pitch = getPitch();

    // Update UI (Thread Safe)
    lvgl_port_lock(-1);
    updateUI(roll, pitch);
    
    lvgl_port_unlock();
    
    // Check for calibration trigger (IMU flat detection or button)
    // Perform NVS write OUTSIDE the LVGL lock to prevent blocking/WDT
    if (consumeCalibrationTrigger()) {
        Serial.println("Calibration Triggered! (RAM Update)");
        // delay(100); // No longer needed for RAM update
        zeroIMU(); 
        yield(); 
        
        // Schedule NVS Save for later (when UI is idle/safe)
        save_cal_pending = true;
        save_cal_timer = millis();
    }
    
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
        uint32_t release_time = millis();
        uint32_t press_duration = release_time - btn_press_start;
        
        // Short Press Logic (< 1000ms)
        if (press_duration < 1000 && !btn_long_press_handled) {
             // Single Click Action: Turn off AP Mode if active
             if (isAPMode()) {
                 stopAPMode();
                 // showToast("WiFi Off"); // stopAPMode usually handles UI? Or needs toast?
                 // Original code didn't toast on stop? 
                 // Let's add feedback if useful, but keep it minimal as requested.
             }
        }
    }
    
    btn_last_state = btn_state;
    
    // --- Background NVS Save (Deferred) ---
    if (save_cal_pending && millis() - save_cal_timer > 3000) {
        Serial.println("Saving Calibration to NVS (Safe Time)...");
        saveIMUOffsets();
        save_cal_pending = false;
        // showToast("Settings Saved"); // Removed as requested
    }

    delay(20); // 50Hz Update Rate (Faster for button response)
}
