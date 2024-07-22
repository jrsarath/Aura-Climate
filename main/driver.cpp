#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <sgp40.h>
#include <driver/gpio.h>
#include "bsp/esp-bsp.h"

#include "includes/driver.h"
#include "includes/sensors.h"
#include "includes/variables.h"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace chip::app::Clusters;

static const char *TAG = "driver";

AirQuality::AirQualityEnum map_voc_index(uint16_t vocIndex) {
    if (vocIndex <= 50) {
        return AirQuality::AirQualityEnum::kGood;
    } else if (vocIndex <= 100) {
        return AirQuality::AirQualityEnum::kFair;
    } else if (vocIndex <= 150) {
        return AirQuality::AirQualityEnum::kModerate;
    } else if (vocIndex <= 200) {
        return AirQuality::AirQualityEnum::kPoor;
    } else if (vocIndex <= 300) {
        return AirQuality::AirQualityEnum::kVeryPoor;
    } else {
        return AirQuality::AirQualityEnum::kExtremelyPoor;
    }
}

void update_matter_with_dht_values() {
    // Update temperature values
    esp_matter_attr_val_t temperature_value;
    temperature_value = esp_matter_invalid(NULL);
    temperature_value.type = esp_matter_val_type_t::ESP_MATTER_VAL_TYPE_UINT16;
    temperature_value.val.u16 = get_temperature() * 100;
    esp_matter::attribute::update(temperature_endpoint_id, TemperatureMeasurement::Id, TemperatureMeasurement::Attributes::MeasuredValue::Id, &temperature_value);
    // Update humidity values
    esp_matter_attr_val_t humidity_value;
    humidity_value = esp_matter_invalid(NULL);
    humidity_value.type = esp_matter_val_type_t::ESP_MATTER_VAL_TYPE_UINT16;
    humidity_value.val.u16 = get_humidity() * 100;
    esp_matter::attribute::update(humidity_endpoint_id, RelativeHumidityMeasurement::Id, RelativeHumidityMeasurement::Attributes::MeasuredValue::Id, &humidity_value);
}
void update_matter_with_sgp_values() {
    // Update voc values
    AirQuality::AirQualityEnum airQuality = map_voc_index(get_voc_index());
    esp_matter_attr_val_t air_quality_value;
    air_quality_value = esp_matter_invalid(NULL);
    air_quality_value.type = esp_matter_val_type_t::ESP_MATTER_VAL_TYPE_ENUM8;
    air_quality_value.val.u8 = static_cast<uint8_t>(airQuality);
    esp_matter::attribute::update(voc_endpoint_id, AirQuality::Id, AirQuality::Attributes::AirQuality::Id, &air_quality_value);
}

esp_err_t sensor_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data) {
    if (type == PRE_UPDATE) {
        if (cluster_id == TemperatureMeasurement::Id) {
            // ESP_LOGI(TAG, "Temperature attribute updated to %d", val->val.i16);
        } else if (cluster_id == RelativeHumidityMeasurement::Id) {
            // ESP_LOGI(TAG, "Humidity attribute updated to %d", val->val.i16);
        } else if (cluster_id == AirQuality::Id) {
            // ESP_LOGI(TAG, "AQI attribute updated to %d", val->val.i16);
        }
    }
    return ESP_OK;
}
esp_err_t driver_attribute_update(driver_handle driver_handle, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
    esp_err_t err = ESP_OK;
    return err;
}

static void dht_task(void *pvParameter) {
    ESP_LOGI(TAG, "Starting DHT task");
    for (;;) {
        read_dht_sensor_data();
        update_matter_with_dht_values();
        vTaskDelay(pdMS_TO_TICKS(20000));
    }
}
static void sgp_task(void *pvParameter) {
    ESP_LOGI(TAG, "Starting SGP task");
    TickType_t last_wakeup = xTaskGetTickCount();
    for (;;) {
        read_sgp_sensor_data();
        update_matter_with_sgp_values();
        vTaskDelayUntil(&last_wakeup, pdMS_TO_TICKS(1000));
    }
}
static void driver_button_toggle_cb(void *arg, void *data) {
    ESP_LOGI(TAG, "Toggle button pressed");
    update_matter_with_dht_values();
    update_matter_with_sgp_values();
}

driver_handle driver_dht_init() {
    ESP_LOGI(TAG, "Initialising DHT driver");
    xTaskCreate(dht_task, "DHT22TASK", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
    return ESP_OK;
}
driver_handle driver_voc_init() {
    ESP_LOGI(TAG, "Initialising VOC driver");
    ESP_ERROR_CHECK(i2cdev_init());
    memset(&sgp, 0, sizeof(sgp));
    ESP_ERROR_CHECK(sgp40_init_desc(&sgp, (i2c_port_t)0, (gpio_num_t)CONFIG_GPIO_I2C_MASTER_SDA, (gpio_num_t)CONFIG_GPIO_I2C_MASTER_SCL));
    ESP_ERROR_CHECK(sgp40_init(&sgp));
    ESP_LOGI(TAG, "SGP40 initilalized. Serial: 0x%04x%04x%04x", sgp.serial[0], sgp.serial[1], sgp.serial[2]);
    // Wait until all set up
    vTaskDelay(pdMS_TO_TICKS(250));

    xTaskCreate(sgp_task, "SGP40TASK", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
    return ESP_OK;
}
driver_handle driver_button_init() {
    button_handle_t btns[BSP_BUTTON_NUM];
    ESP_ERROR_CHECK(bsp_iot_button_create(btns, NULL, BSP_BUTTON_NUM));
    ESP_ERROR_CHECK(iot_button_register_cb(btns[0], BUTTON_PRESS_DOWN, driver_button_toggle_cb, NULL));
    return (driver_handle)btns[0];
}