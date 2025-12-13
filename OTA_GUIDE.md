# OTA Update Guide

You can now update the Tacomometer firmware wirelessly.

## Prerequisites
1.  **Compile Firmware**: In Arduino IDE, go to **Sketch -> Export Compiled Binary**. This will create a `.bin` file in your project folder (`build/...` or directly in the folder depending on version).
2.  **Connect**: Connect your device (laptop/phone) to the dashboard gauge WiFi (`Tacomometer`).

## Steps to Update
1.  Open the Web UI (`http://192.168.4.1`).
2.  Click **Device Settings**.
3.  Scroll down to the **Firmware Update** section.
4.  Click **Select .bin** and choose the exported firmware file.
5.  Click **Flash**.
6.  The device will upload the file, show progress, and restart automatically.

## Troubleshooting
-   **Upload Failed**: Ensure you are close to the device for a strong WiFi signal.
-   **Wrong File**: Ensure you selected the `.bin` file, NOT the `.elf` or `.ino` file.
