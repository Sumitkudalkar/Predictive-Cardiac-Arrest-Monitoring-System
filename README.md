# ESP8266 Health & Fall Detection Smartwatch

*A wearable prototype developed for a hackathon, designed to monitor vital signs and detect falls, featuring a local TFT display and a remote web dashboard for real-time tracking and alerts.*


## 📖 About The Project

Well currently we have made it only for haelthy people.Using ML we can scale it to larger extent.

The device provides at-a-glance information on a local touchscreen display and serves a web-based dashboard accessible to caregivers or family members on the same local network. The system is designed to detect two critical events: **sudden falls** (via an accelerometer) and **abnormal vital signs** (low heart rate or SpO2), triggering a multi-stage alert system to draw immediate attention.

## ✨ Key Features

* **Real-time Vitals Monitoring**: Displays Heart Rate (BPM), SpO2, Temperature, and Humidity.
* **Fall Detection**: Uses accelerometer data to detect a fall based on a G-force magnitude threshold.
* **Multi-Screen TFT Display**:
    * **Clock Screen**: Shows the current time and date.
    * **Health Status Screen**: Displays all sensor readings in an organized layout.
    * **Alert Screens**: Dedicated, high-visibility alerts for falls and abnormal vitals.
* **Multi-Stage Alert System**:
    1.  A flashing **Medical/Fall Alert** is triggered.
    2.  If the medical alert is not dismissed within 7 seconds, it escalates to a **Critical Emergency** screen, indicating that loved ones are being notified.(The notification is to be added we need an app for it)
* **Remote Web Dashboard**:
    * **Main Dashboard (`/`)**: A clean interface showing live patient data.
    * **Family Dashboard (`/fameli`)**: A simplified view that provides "All okay" or "Abnormal" status messages.
* **Touchscreen Interface**: Simple navigation between screens using a touch-sensitive button area.

## ⚙️ How It Works

The system operates on a master-slave principle:

1.  **Data Acquisition**: A slave microcontroller (not included in this code) is assumed to be gathering sensor data (Heart Rate, SpO2, Temperature, Humidity, and one accelerometer axis). It sends this data as a single comma-separated string over the serial port.
2.  **Master ESP8266**: The ESP8266 in this project acts as the master controller.
    * It reads the serial data string in each loop.
    * It reads its own onboard analog accelerometer for the second axis.
    * It calculates the total G-force magnitude to check for falls (`mag > 1.3`).
    * It hosts a WiFi Web Server, providing the HTML pages and a `/data` JSON endpoint.
    * It manages the device's state (which screen to show) based on sensor data and user touch input.
    * It drives the TFT display, rendering the appropriate UI for the current state.

## 🛠️ Hardware & Software

### Hardware Required

* **ESP8266 Development Board** (e.g., NodeMCU V2)
* **TFT Touchscreen Display** (e.g., a 2.4" ILI9341-based display)
* **Analog Accelerometer** (e.g., ADXL335, connected to the A0 pin of the ESP8266)
* **(Slave MCU)** An additional microcontroller (like an Arduino Nano) with the following sensors:
    * **MAX30102** or similar for Heart Rate and SpO2.
    * **DHT11/DHT22** for Temperature and Humidity.
    * Another accelerometer axis (optional).
