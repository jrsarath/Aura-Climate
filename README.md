<img src="assets/aura.svg" width="100%" />

# Aura Climate
A simple firmware using the esp-matter SDK to develop a Matter-enabled device with the following features:
- **Temperature & Humidity:** DHT22 sensor
- **Air Quality:** SGP40 sensor

The firmware provides a flexible design to easily enable or disable sensors and features based on your requirements.

## Building and flashing
Requirements
- ESP-IDF installed and configured
- esp-matter installed and configured
Steps
- Clone the repository
- Set GPIO pins for DHT22 and I2C (SGP40) sensors using Kconfig
- Build and flash the firmware


| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

## Demo device
This project includes a custom PCB integrating a DHT22/AM2302 from ASAIR and SGP40 from Adafruit, with a Hi-Link converter to run the device from AC. The board is powered by an ESP32 Devkit.
- [PCB Schema](https://365.altium.com/files/E2252F43-3197-4BE0-AAA4-C608606C2910)
- [Demo Device](assets/device.jpg)

## Other firmwares
- [Aura Control](https://github.com/jrsarath/aura-control)


## Credits
- [esp-idf](https://github.com/espressif/esp-idf)
- [esp-matter](https://github.com/espressif/esp-matter)
- [esp-idf-lib](https://github.com/UncleRus/esp-idf-lib/)

<br />
<br />
Made in Kolkata with ❤️ 