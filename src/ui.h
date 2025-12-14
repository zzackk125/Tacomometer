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
bool consumeCalibrationTrigger(); // One-shot accessor
void showToast(const char* text, uint32_t duration_ms = 2000);
void hideToast();
void triggerCalibrationUI();

// Settings API
void setUIRotation(int degrees);
int getUIRotation();
void setCriticalValues(int roll, int pitch);
int getCriticalRoll();
int getCriticalPitch();
void setUIColor(int color_idx);
int getUIColor();
void setPixelShift(bool enabled);
bool getPixelShift();
void resetSettings();

