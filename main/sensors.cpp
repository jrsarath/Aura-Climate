#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_system.h>
#include <dht.h>
#include <sgp40.h>

#include "includes/sensors.h"
#include "includes/variables.h"

#define SENSOR_TYPE DHT_TYPE_AM2301

static const char *TAG = "sensors";

float get_temperature() {
    return temperature;
}
float get_humidity() {
    return humidity;
}
int32_t get_voc_index() {
    return voc_index;
}

esp_err_t read_dht_sensor_data() {
    if (dht_read_float_data(SENSOR_TYPE, (gpio_num_t)CONFIG_GPIO_DHT22_GPIO_PIN, &humidity, &temperature) != ESP_OK) {
        humidity = 0.0;
        temperature = 0.0;
        ESP_LOGE(TAG, "DHT22 Sensor not found");
    }
    return ESP_OK;
}
esp_err_t read_sgp_sensor_data() {
    if (sgp40_measure_voc(&sgp, 0.00, 0.00, &voc_index) != ESP_OK) {
        voc_index = 0;
        ESP_LOGE(TAG, "SGP40 Sensor error");
    }
    return ESP_OK;
}

void log_data() {
    while (1) {
        ESP_LOGI(TAG, "Temperature: %f", temperature);
        ESP_LOGI(TAG, "Humidity: %f", humidity);
        ESP_LOGI(TAG, "AQI: %ld", voc_index);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void initiate_dht() {
    ESP_LOGI(TAG, "Initialising DHT22 sensor");
}
void initiate_sgp() {
    ESP_LOGI(TAG, "Initialising SGP40 sensors");
}