/*
 * File: ui.h
 * Description: UI Logic Interface
 * Author: zzackk125
 * License: MIT
 */

#pragma once

#include <lvgl.h>

void initUI();
void updateUI(float roll, float pitch);
bool isCalibrating();
