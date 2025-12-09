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
}

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

    delay(50); // 20Hz Update Rate
}
