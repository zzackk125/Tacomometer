#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "SensorQMI8658.hpp"

void initIMU();
void updateIMU();
float getRoll();
float getPitch();
void zeroIMU();
