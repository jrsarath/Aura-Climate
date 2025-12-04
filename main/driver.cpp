#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <esp_timer.h>
#include <device.h>
#include <driver/gpio.h>

#include "includes/driver.h"
#include "includes/sensors.h"
#include "includes/variables.h"
#include "includes/config.h"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace chip::app::Clusters;

static const char *TAG = "driver";

AirQuality::AirQualityEnum map_voc_index(uint16_t vocIndex) {
    if (esp_timer_get_time() / 1000 - sgp40_start_time_ms < SGP40_WARMUP_TIME_MS) {
        return AirQuality::AirQualityEnum::kUnknown;
    }

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

void update_matter_with_sensor_values(const SensorManager* sensor_manager) {
    if (!sensor_manager) {
        ESP_LOGE(TAG, "Invalid sensor manager pointer");
        return;
    }

    const DHTSensor* dht = sensor_manager->getDHTSensor();
    const SGP40Sensor* sgp = sensor_manager->getSGP40Sensor();

    if (dht && dht->validateReading()) {
        float temp = dht->getTemperature();
        float humidity = dht->getHumidity();
        
        // Update temperature values
        esp_matter_attr_val_t temperature_value = esp_matter_invalid(NULL);
        temperature_value.type = esp_matter_val_type_t::ESP_MATTER_VAL_TYPE_INT16;
        temperature_value.val.i16 = static_cast<int16_t>(temp * 100);
        ESP_LOGI(TAG, "Updating Matter temperature: %.2f°C (raw: %d)", temp, temperature_value.val.i16);
        esp_matter::attribute::update(temperature_endpoint_id, 
                                    TemperatureMeasurement::Id, 
                                    TemperatureMeasurement::Attributes::MeasuredValue::Id, 
                                    &temperature_value);

        // Update humidity values
        esp_matter_attr_val_t humidity_value = esp_matter_invalid(NULL);
        humidity_value.type = esp_matter_val_type_t::ESP_MATTER_VAL_TYPE_UINT16;
        humidity_value.val.u16 = static_cast<uint16_t>(humidity * 100);
        ESP_LOGI(TAG, "Updating Matter humidity: %.2f%% (raw: %u)", humidity, humidity_value.val.u16);
        esp_matter::attribute::update(humidity_endpoint_id, 
                                    RelativeHumidityMeasurement::Id, 
                                    RelativeHumidityMeasurement::Attributes::MeasuredValue::Id, 
                                    &humidity_value);
    }

    if (sgp && sgp->validateReading()) {
        // Update VOC values
        AirQuality::AirQualityEnum airQuality = map_voc_index(sgp->getVOCIndex());
        esp_matter_attr_val_t air_quality_value = esp_matter_invalid(NULL);
        air_quality_value.type = esp_matter_val_type_t::ESP_MATTER_VAL_TYPE_ENUM8;
        air_quality_value.val.u8 = static_cast<uint8_t>(airQuality);
        ESP_LOGI(TAG, "Updating Matter VOC: index %ld (air quality: %d)", sgp->getVOCIndex(), air_quality_value.val.u8);
        esp_matter::attribute::update(voc_endpoint_id, 
                                    AirQuality::Id, 
                                    AirQuality::Attributes::AirQuality::Id, 
                                    &air_quality_value);
    }
}

static void driver_button_toggle_cb(void *arg, void *data) {
    ESP_LOGI(TAG, "Toggle button pressed");
    SensorManager* sensor_manager = static_cast<SensorManager*>(data);
    if (sensor_manager) {
        update_matter_with_sensor_values(sensor_manager);
    }
}

driver_handle driver_button_init(void* sensor_manager) {
    button_gpio_config_t config = button_driver_get_config();
    button_dev_t* btn_dev = NULL;

    // Prepare a generic button configuration. Zero values will let the
    // button implementation fall back to defaults if applicable.
    button_config_t btn_cfg = {0};

    // Create a GPIO-based button device using the dedicated helper
    // which accepts a gpio config struct. The previous code passed the
    // gpio config incorrectly to `iot_button_create`, causing
    // ESP_ERR_INVALID_ARG at runtime.
    esp_err_t rc = iot_button_new_gpio_device(&btn_cfg, &config, &btn_dev);
    if (rc == ESP_OK && btn_dev) {
        iot_button_register_cb(btn_dev, BUTTON_PRESS_DOWN, NULL, driver_button_toggle_cb, sensor_manager);
    } else {
        ESP_LOGE(TAG, "Failed to create GPIO button device: %d", rc);
    }
    return (driver_handle)btn_dev;
}

void device_identifier_cb() {
    gpio_set_direction((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, GPIO_PULLUP_ONLY);

    for (int blink_count = 0; blink_count < 6; blink_count++) {
        gpio_set_level((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    gpio_set_level((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, 0);
}

static TaskHandle_t led_task_handle = nullptr;
static bool commission_mode = false;

static void led_blink_task(void* pvParameters) {
    while (commission_mode) {
        gpio_set_level((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, 1);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        gpio_set_level((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, 0);
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    gpio_set_level((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, 0);
    vTaskDelete(NULL);
    led_task_handle = nullptr;
}

void device_commission_window_open_cb() {
    gpio_set_direction((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode((gpio_num_t)CONFIG_GPIO_INDICATOR_LED, GPIO_PULLUP_ONLY);
    
    commission_mode = true;
    if (led_task_handle == nullptr) {
        xTaskCreate(led_blink_task, "led_task", 2048, NULL, 1, &led_task_handle);
    }
}

void device_commission_window_close_cb() {
    commission_mode = false;
    // Task will clean itself up
}