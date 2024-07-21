![Matter](https://csa-iot.org/wp-content/uploads/2022/09/matter_lkup_rgb_night-scaled.jpg)
# Aura Climate
A simple firmware using esp-matter sdk to create a matter enabled device that has three features/sensors
- Temperature & Humidty - courtesy of DHT22
- Air Quality - courtesy of SGP40

Firmware provides flexible design to easily enable/disable sensors/features based on requirement.

## Building and flashing
Requirements
- esp-idf installed and configured
- esp-matter installed and configured
Steps
- clone the repository
- set gpio pins for DHT22 and i2C (SGP40) for sensors using Kconfig
- make/adjust features as per requirement
- build and flash


| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

## Demo device
I have create a custom pcb to integrate a DHT22/AM2302 from ASAIR & SGP40 from Adafruit with a hi-link converter to run the device from AC. the board is powered by an ESP32 Devkit
- [PCB Schema](https://365.altium.com/files/E2252F43-3197-4BE0-AAA4-C608606C2910)
- [Demo Device](device.jpg)

## Folder structure

The project **sample_project** contains source files written in C++, the entrypoint file is [main.cpp](main/main.cpp). All the source files are located in folder [main](main).

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt`
files that provide set of directives and instructions describing the project's source files and targets
(executable, library, or both). 

Below is short explanation of remaining files in the project folder.

```
├── CMakeLists.txt
├── Makefile
├── main
│   ├── CMakeLists.txt
│   ├── drivers.cpp
│   ├── globals.cpp
│   ├── inlcudes
│   │   ├── common.h
│   │   ├── driver.h
│   │   ├── sensors.h
│   │   ├── utils.h
│   │   ├── variables.h
│   ├── idf_components.yml
│   ├── Kconfig.projbuild
│   └── main.cpp
│   └── sensors.cpp
├── partitions.csv
├── sdkconfig.defaults
├── sdkconfig.defaults.c6_thread
├── sdkconfig.defaults.esp32c2
├── sdkconfig.defaults.esp32c2.v5.1
├── sdkconfig.defaults.esp32c2.v5.2
├── sdkconfig.defaults.esp32c3
├── sdkconfig.defaults.esp32c6
├── sdkconfig.defaults.esp32h2
├── sdkconfig.defaults.esp32s2
├── sdkconfig.defaults.esp32s3
└── README.md                  This is the file you are currently reading
```
Additionally, the project contains Makefile and component.mk files, used for the legacy Make based build system. 
They are not used or needed when building with CMake and idf.py.

## Credits
- [esp-idf](https://github.com/espressif/esp-idf)
- [esp-matter](https://github.com/espressif/esp-matter)
- [esp-idf-lib](https://github.com/UncleRus/esp-idf-lib/)
