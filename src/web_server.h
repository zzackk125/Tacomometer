/*
 * File: web_server.h
 * Description: Web Server and Wi-Fi AP Interface
 * Author: zzackk125
 * License: MIT
 */

#pragma once

#include <Arduino.h>

#define FIRMWARE_VERSION "1.0.0"

void initWebServer();
void createWebServer();
void handleWebServer();
void startAPMode();
void stopAPMode();
bool isAPMode();

// Config API
void setWiFiTimeout(int seconds);
int getWiFiTimeout();
