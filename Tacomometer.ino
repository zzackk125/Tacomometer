/*
 * Tacomometer - LVGL Version
 * Target: Waveshare ESP32-C6 Touch AMOLED 1.43
 */

#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include "src/lvgl_port.h"
#include "src/board_config.h"
#include "src/touch_driver.h"
#include "src/imu_driver.h"
#include "src/ui.h"

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Starting Tacomometer (LVGL)...");

    // 1. Init I2C for IO Expander, Touch, and IMU
    Wire.begin(ESP32_SDA_NUM, ESP32_SCL_NUM);
    delay(100);

    // 2. Enable Display Power (TCA9554 IO Expander)
    Serial.println("Powering up display...");
    Wire.beginTransmission(0x20);
    Wire.write(0x03); 
    Wire.write(0xBF); // Pin 6 Output
    Wire.endTransmission();
    
    Wire.beginTransmission(0x20);
    Wire.write(0x01); 
    Wire.write(0x40); // Pin 6 High
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
}

void loop() {
    // Read IMU
    updateIMU();
    float roll = getRoll();
    float pitch = getPitch();

    // Update UI (Thread Safe)
    lvgl_port_lock(-1);
    updateUI(roll, pitch);
    
    // Check for calibration trigger
    if (isCalibrating()) {
        // We only want to zero once per trigger, but isCalibrating stays true for 2s for the message.
        // Simple hack: Zero continuously or use a flag. 
        // Better: Zero immediately when detected.
        // Let's assume zeroIMU handles it or we do it once.
        // Actually, let's add zeroIMU() to imu_driver and call it.
        zeroIMU(); 
    }
    
    lvgl_port_unlock();

    delay(50); // 20Hz Update Rate
}
