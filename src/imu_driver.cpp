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
    Serial.printf("Loaded Offsets: Roll=%f, Pitch=%f\n", offsetRoll, offsetPitch);
}

void updateIMU() {
    if (qmi.getDataReady()) {
        if (qmi.getAccelerometer(acc.x, acc.y, acc.z)) {
            // Calculate Roll and Pitch (Simple Trig)
            float rawRoll = atan2(acc.y, acc.z) * 180.0 / PI;
            float rawPitch = atan2(-acc.x, sqrt(acc.y * acc.y + acc.z * acc.z)) * 180.0 / PI;
            
            currentRoll = rawRoll - offsetRoll;
            currentPitch = rawPitch - offsetPitch;
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
    
    // Save to NVS
    prefs.putFloat("roll_off", offsetRoll);
    prefs.putFloat("pitch_off", offsetPitch);
    Serial.println("Offsets Saved to NVS");
    
    // Reset current to 0 immediately for visual feedback
    currentRoll = 0;
    currentPitch = 0;
}
