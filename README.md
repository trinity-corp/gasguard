# GasGuard
GasGuard is a firmware project for ESP32 that demonstrates how to read data from an MQ-7 gas sensor and interact with the [EcoMonitor](https://github.com/trinity-corp/ecomonitor) API.
The project shows the general workflow but is not intended for accurate CO measurement.

# Features
- Reads CO value using MQ-7 sensor
- Sends sensor data over Wi-Fi to the [EcoMonitor](https://github.com/trinity-corp/ecomonitor) API.
- Processes commands sent from the [EcoMonitor](https://github.com/trinity-corp/ecomonitor) API.
  - Disable/enable screen
  - Reboot the device
  - Clear configuration (factory reset)
  - Change sensor reading interval
- Displays status messages on an I2C LCD
- Creates its own access point for user configuration
- Allows the user to configure Wi-Fi credentials and reading interval
- Stores configuration settings in NVS

# Important Notice

MQ-7 provides very approximate CO readings. The measured values depend on:
- temperature
- humidity
- heating cycle timing
- calibration
- supply voltage stability

The ppm value produced by this firmware must not be used for safety-critical CO monitoring.
In this project, the readings are used only as an example of how gas sensor data can be transmitted.

# Purpose

This project was made by Andriy Tymchuk for МАН (Мала Академія Наук), as a computer science project. The futher ideas of project is intended for learning, experiments, science projects, and demonstrating how microcontrollers interact with gas sensors and network services.

# Other Guard devices
- [TempGuard](https://github.com/trinity-corp/tempguard) - temperature reading device
- [HumidGuard](https://github.com/trinity-corp/humidguard) - humidity reading device
