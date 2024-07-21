#pragma once

#include "esp_system.h"
#include <freertos/FreeRTOS.h>

#define SENSORS_H

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t read_dht_sensor_data();
esp_err_t read_sgp_sensor_data();

float get_temperature();
float get_humidity();
int32_t get_voc_index();

void initiate_dht();
void initiate_sgp();

#ifdef __cplusplus
}
#endif