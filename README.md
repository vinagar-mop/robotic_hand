# ESP32 Robotic Hand

A Wi-Fi-controlled robotic hand running on the ESP32. Hosts an embedded web interface that lets you control five servo-driven fingers individually or via preset gestures (point, rock on, peace), with a live 3D model of the hand in the browser.

## Requirements
1. The systems must run on a web  server that a user can access through an AP.
2. The system must allow every command from the web page to be registered.
3. The system shall allow users to open and close fingers together.
4. The system shall allow users to close and open their hands all at once.
5. The system shall also allow pre-programmed commands for finger movements.


## Features

- Wi-Fi SoftAP — connect directly to the ESP32, no router needed
- Embedded web server with a 3D hand viewport (Three.js)
- Per-finger angle control (0° closed → 180° open)
- Preset gestures: Point, Rock On, Peace
- "Open All" / "Close All" quick controls

## Hardware

- ESP32 dev board
- 5 × servos (one per finger)
- Robotic hand mechanism

## Installation

This project uses **ESP-IDF** (tested with v5.5.1).

1. **Install ESP-IDF** following the official guide: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/
2. **Clone this repo:**
3. **Open the project folder in ESP-IDF.** From the ESP-IDF terminal/PowerShell, navigate to the project folder, or use the ESP-IDF VS Code extension and open the folder directly.
4. **Build, flash, and monitor:**
idf.py build
idf.py -p <COM_PORT> flash monitor
Replace `<COM_PORT>` with your ESP32's serial port (e.g. `COM3` on Windows, `/dev/ttyUSB0` on Linux).

## Usage

1. After flashing, the ESP32 broadcasts a Wi-Fi SoftAP (SSID set in `wifi_app.c`).
2. Connect your phone or laptop to that network.
3. Open `http://192.168.0.1` (or whatever IP the ESP32 logs on boot) in a browser.
4. Use the web interface to control the hand.
