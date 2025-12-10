/*
 * File: imu_driver.h
 * Description: IMU (QMI8658) Driver Interface
 * Author: zzackk125
 * License: MIT
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "SensorQMI8658.hpp"

void initIMU();
void updateIMU();
float getRoll();
float getPitch();
void zeroIMU(); // Updates RAM offsets only
void saveIMUOffsets(); // Explicitly save to NVS
