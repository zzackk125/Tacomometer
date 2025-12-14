/*
 * File: imu_driver.cpp
 * Description: IMU (QMI8658) Driver Implementation
 * Author: zzackk125
 * License: MIT
 */

#include "imu_driver.h"
#include "board_config.h"

SensorQMI8658 qmi;
IMUdata acc;
IMUdata gyr;

float currentRoll = 0.0;
float currentPitch = 0.0;

#include <Preferences.h>

Preferences prefs;

float offsetRoll = 0.0;
float offsetPitch = 0.0;

// Smoothing
float smoothing_alpha = 1.0; // 1.0 = No smoothing (Instant), 0.1 = Heavy smoothing
int smoothing_percent = 100; // 100% = Instant

float smoothRoll = 0.0;
float smoothPitch = 0.0;

// Sensor Fusion
int calc_mode = 0; // 0=Fusion (Default), 1=EMA
float gyroX_offset = 0.0;
float gyroY_offset = 0.0;
uint32_t last_update_time = 0;
// Fusion variables (accumulators)
float fusionRoll = 0.0;
float fusionPitch = 0.0;

void initIMU() {
    // Initialize QMI8658
    // Address is usually 0x6B or 0x6A. Demo used QMI8658_L_SLAVE_ADDRESS which is 0x6B.
    if (!qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, ESP32_SDA_NUM, ESP32_SCL_NUM)) {
        Serial.println("Failed to find QMI8658 - check your wiring!");
    } else {
        Serial.println("QMI8658 Found!");
    }

    // Configure (from demo)
    qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_1000Hz, SensorQMI8658::LPF_MODE_0);
    qmi.configGyroscope(SensorQMI8658::GYR_RANGE_64DPS, SensorQMI8658::GYR_ODR_896_8Hz, SensorQMI8658::LPF_MODE_3);
    qmi.enableGyroscope();
    qmi.enableAccelerometer();
    
    // Load Offsets
    prefs.begin("imu", false); // Namespace "imu", read-only false
    offsetRoll = prefs.getFloat("roll_off", 0.0);
    offsetPitch = prefs.getFloat("pitch_off", 0.0);
    
    // Load Smoothing
    smoothing_percent = prefs.getInt("smooth", 100); // Default 100 (Instant)
    if(smoothing_percent < 1) smoothing_percent = 1;
    if(smoothing_percent > 100) smoothing_percent = 100;
    
    // Convert percent to alpha
    // 100% -> Alpha 1.0
    // 1% -> Alpha 0.01
    smoothing_alpha = (float)smoothing_percent / 100.0;
    
    // Load Calculation Mode
    calc_mode = prefs.getInt("mode", 0); // Default 0 (Fusion)
    
    Serial.printf("Loaded Offsets: Roll=%f, Pitch=%f, Smooth=%d%%, Mode=%d\n", offsetRoll, offsetPitch, smoothing_percent, calc_mode);

    // Calibrate Gyro (Quick Sample)
    Serial.println("Calibrating Gyro (Do not move)...");
    float sumX = 0, sumY = 0;
    int samples = 100;
    for(int i=0; i<samples; i++) {
        if(qmi.getDataReady()) {
            if(qmi.getGyroscope(gyr.x, gyr.y, gyr.z)) {
                sumX += gyr.x;
                sumY += gyr.y;
            }
        }
        delay(3);
    }
    gyroX_offset = sumX / samples;
    gyroY_offset = sumY / samples;
    Serial.printf("Gyro Calibrated: Xoff=%f, Yoff=%f\n", gyroX_offset, gyroY_offset);
    
    last_update_time = micros();
}

void updateIMU() {
    if (qmi.getDataReady()) {
        if (qmi.getAccelerometer(acc.x, acc.y, acc.z)) {
            // Calculate Roll and Pitch (Simple Trig)
            float rawRoll = atan2(acc.y, acc.z) * 180.0 / PI;
            float rawPitch = atan2(-acc.x, sqrt(acc.y * acc.y + acc.z * acc.z)) * 180.0 / PI;
            
            float targetRoll = rawRoll - offsetRoll;
            float targetPitch = rawPitch - offsetPitch;
            
            // Calculate dt
            uint32_t now = micros();
            float dt = (now - last_update_time) / 1000000.0; // Seconds
            last_update_time = now;
            
            // --- MODE 0: SENSOR FUSION (Complementary Filter) ---
            if (calc_mode == 0) {
                 // Get Gyro data (Degrees Per Second)
                 qmi.getGyroscope(gyr.x, gyr.y, gyr.z);
                 
                 // Apply offsets
                 float gx = gyr.x - gyroX_offset;
                 float gy = gyr.y - gyroY_offset;
                 
                 // Complementary Filter
                 // Angle = 0.98 * (Angle + Gyro * dt) + 0.02 * AccelAngle
                 // Note: Accel Roll uses Y/Z, so rotation is around X axis -> Gyro X
                 // Accel Pitch uses X/Y/Z, rotation around Y axis -> Gyro Y
                 
                 // We use a fixed high-alpha for fusion (0.98 is standard)
                 // But we can let the user's "Smoothing" slider slightly adjust the trust?
                 // Standard practice: Fixed 0.98 is usually best.
                 float alpha = 0.98;
                 
                 // Use previous FUSED value for integration
                 // Check for NaN or first run
                 if(fusionRoll == 0.0 && fusionPitch == 0.0 && targetRoll != 0) {
                     fusionRoll = targetRoll; // Seed with accel
                     fusionPitch = targetPitch;
                 }

                 fusionRoll = alpha * (fusionRoll + gx * dt) + (1.0 - alpha) * targetRoll;
                 // Pitch is often inverted on gyro depending on mounting, checking simple addition first
                 fusionPitch = alpha * (fusionPitch + gy * dt) + (1.0 - alpha) * targetPitch;
                 
                 smoothRoll = fusionRoll;
                 smoothPitch = fusionPitch;
                 
            } 
            // --- MODE 1: EMA (Original) ---
            else {
                // current = alpha * target + (1-alpha) * current
                if (smoothing_alpha >= 0.99) {
                     smoothRoll = targetRoll;
                     smoothPitch = targetPitch;
                } else {
                     smoothRoll = (smoothing_alpha * targetRoll) + ((1.0 - smoothing_alpha) * smoothRoll);
                     smoothPitch = (smoothing_alpha * targetPitch) + ((1.0 - smoothing_alpha) * smoothPitch);
                }
                
                // Keep fusion synced so if we switch modes it doesn't jump
                fusionRoll = smoothRoll;
                fusionPitch = smoothPitch;
            }
            
            currentRoll = smoothRoll;
            currentPitch = smoothPitch;
        }
    }
}

float getRoll() {
    return currentRoll;
}

float getPitch() {
    return currentPitch;
}

void zeroIMU() {
    // Capture current raw values as offsets
    // We need to reconstruct raw from current + offset
    offsetRoll += currentRoll;
    offsetPitch += currentPitch;
    
    // RAM ONLY: Do NOT save to NVS here. 
    // Saving to NVS blocks interrupts and causes freezes if done in UI loop.
    // We rely on auto-save or explicit save later.
    Serial.println("Offsets Updated (RAM)");
    
    // Reset current to 0 immediately for visual feedback
    currentRoll = 0;
    currentPitch = 0;
}

void saveIMUOffsets() {
    prefs.putFloat("roll_off", offsetRoll);
    prefs.putFloat("pitch_off", offsetPitch);
    Serial.println("Offsets Saved to NVS (Background)");
}

void setSmoothing(int percent) {
    if(percent < 1) percent = 1;
    if(percent > 100) percent = 100;
    
    smoothing_percent = percent;
    smoothing_alpha = (float)smoothing_percent / 100.0;
    
    prefs.putInt("smooth", smoothing_percent);
}

int getSmoothing() {
    return smoothing_percent;
}

void setCalculationMode(int mode) {
    if (mode < 0) mode = 0;
    if (mode > 1) mode = 1;
    calc_mode = mode;
    prefs.putInt("mode", calc_mode);
    Serial.printf("Calculation Mode Set to: %d\n", calc_mode);
}

int getCalculationMode() {
    return calc_mode;
}
