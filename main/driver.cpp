#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include "bsp/esp-bsp.h"
#include <esp_matter.h>
#include <inttypes.h>
#include <driver/gpio.h>
#include <button_gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "includes/config.hpp"
#include "includes/variables.hpp"
#include "includes/driver.hpp"
#include "includes/sensors.hpp"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace chip::app::Clusters;

static const char *TAG = "driver";
// Identification pulse state (driver-side)
static TaskHandle_t s_ident_task_drv = NULL;
static volatile bool s_ident_running_drv = false;
static uint16_t s_ident_count_drv = 0;
static gpio_num_t s_ident_gpio_drv = GPIO_NUM_NC;
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


/**
 * @brief Identification task for driver
 * 
 * @param arg 
 */
static void identification_task_drv(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "Driver identification task started (blinks=%u)", s_ident_count_drv);

    const uint32_t on_ms = 2000;

    int orig_level = -1;
    if (s_ident_gpio_drv != GPIO_NUM_NC) {
        // ensure gpio is output so we can set level
        gpio_set_direction(s_ident_gpio_drv, GPIO_MODE_OUTPUT);
        orig_level = gpio_get_level(s_ident_gpio_drv);
    }

    // Single simple toggle: set opposite, wait 2s, restore original
    if (s_ident_gpio_drv != GPIO_NUM_NC && orig_level >= 0) {
        ESP_LOGI(TAG, "Driver identification task, Setting GPIO %d to %d", s_ident_gpio_drv, !orig_level);
        gpio_set_level(s_ident_gpio_drv, !orig_level);
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        ESP_LOGI(TAG, "Driver identification task, Returning GPIO %d to %d", s_ident_gpio_drv, orig_level);
        gpio_set_level(s_ident_gpio_drv, orig_level);
    }

    ESP_LOGI(TAG, "Driver identification task stopping");
    s_ident_running_drv = false;
    TaskHandle_t t = s_ident_task_drv;
    s_ident_task_drv = NULL;
    if (t) vTaskDelete(NULL);
}

/**
 * @brief Input button callback
 * 
 * @param arg 
 * @param data 
 */
static void driver_button_toggle_cb(void *arg, void *data) {
    ESP_LOGI(TAG, "Toggle button pressed");
}

/**
 * @brief Start the driver identification pulse
 * 
 * @param endpoint_id 
 */
void driver_identify_pulse(uint16_t endpoint_id) {
    // cancel previous
    if (s_ident_running_drv) {
        driver_identify_stop();
    }

    uint32_t blinks = 3;

    // TODO: Define different color blink for different sensors
    // gpio_num_t gpio = get_gpio_by_endpoint(endpoint_id);
    // if (gpio == GPIO_NUM_NC) {
    //     ESP_LOGE(TAG, "No GPIO mapping for endpoint %d", endpoint_id);
    //     return;
    // }

    // s_ident_gpio_drv = gpio;
    // s_ident_count_drv = blinks;
    // s_ident_running_drv = true;

    // BaseType_t created = xTaskCreate(identification_task_drv, "drv_ident", 3072, NULL, tskIDLE_PRIORITY + 1, &s_ident_task_drv);
    // if (created != pdPASS) {
    //     ESP_LOGE(TAG, "Failed to create driver identification task");
    //     s_ident_running_drv = false;
    //     s_ident_task_drv = NULL;
    // }
}

/**
 * @brief Stop the driver identification pulse
 * 
 */
void driver_identify_stop(void) {
    if (!s_ident_running_drv && s_ident_task_drv == NULL) return;
    s_ident_running_drv = false;
    // Wait long enough for the single toggle to finish (on_ms ~= 2000ms)
    const TickType_t wait_ticks = pdMS_TO_TICKS(3000);
    const TickType_t poll_ticks = pdMS_TO_TICKS(50);
    TickType_t waited = 0;
    while (s_ident_task_drv != NULL && waited < wait_ticks) {
        vTaskDelay(poll_ticks);
        waited += poll_ticks;
    }
    if (s_ident_task_drv != NULL) {
        vTaskDelete(s_ident_task_drv);
        s_ident_task_drv = NULL;
    }
    s_ident_running_drv = false;
    s_ident_gpio_drv = GPIO_NUM_NC;
}

/**
 * @brief Update Matter attributes with current sensor values
 * 
 * @param sensor_manager Pointer to the SensorManager instance
 */
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

/**
 * @brief Initialize the driver button.
 * 
 * @return A driver handle for the initialized button.
 */
driver_handle driver_button_init() {
    button_handle_t btns[BSP_BUTTON_NUM];
    ESP_ERROR_CHECK(bsp_iot_button_create(btns, NULL, BSP_BUTTON_NUM));
    ESP_ERROR_CHECK(iot_button_register_cb(btns[0], BUTTON_PRESS_DOWN, NULL, driver_button_toggle_cb, NULL));
    
    return (driver_handle)btns[0];
}