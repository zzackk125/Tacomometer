/*
 * File: web_server.h
 * Description: Web Server and Wi-Fi AP Interface
 * Author: zzackk125
 * License: MIT
 */

#pragma once

#include <Arduino.h>

void initWebServer();
void createWebServer();
void handleWebServer();
void startAPMode();
void stopAPMode();
bool isAPMode();
