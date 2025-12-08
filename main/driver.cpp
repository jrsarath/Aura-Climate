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
#include <app/clusters/air-quality-server/air-quality-server.h>
#include <app/util/attribute-storage.h>
#include <lib/support/BitMask.h>
#include <platform/CHIPDeviceLayer.h>

#include "config.hpp"
#include "variables.hpp"
#include "driver.hpp"
#include "sensors.hpp"
#include "epaper_manager.hpp"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace chip::app::Clusters;

static const char *TAG = "DRIVER";
// Identification pulse state (driver-side)
static TaskHandle_t s_ident_task_drv = NULL;
static volatile bool s_ident_running_drv = false;
static uint16_t s_ident_count_drv = 0;
static gpio_num_t s_ident_gpio_drv = GPIO_NUM_NC;

// Air Quality Instance for managing air quality attributes
static AirQuality::Instance* s_airQualityInstance = nullptr;
AirQuality::AirQualityEnum map_aqi_uba(uint8_t aqi) {
    switch (aqi) {
        case 1: return AirQuality::AirQualityEnum::kGood;
        case 2: return AirQuality::AirQualityEnum::kFair;
        case 3: return AirQuality::AirQualityEnum::kModerate;
        case 4: return AirQuality::AirQualityEnum::kPoor;
        case 5: return AirQuality::AirQualityEnum::kExtremelyPoor;
        default: return AirQuality::AirQualityEnum::kUnknown;
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
 * @brief Initialize the Air Quality Instance
 * 
 * @param endpoint_id The endpoint ID for the air quality sensor
 * @return esp_err_t ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t driver_air_quality_init(uint16_t endpoint_id) {
    // If the Air Quality cluster is not present on this endpoint, skip init to avoid abort
    if (!emberAfContainsServer(chip::EndpointId(endpoint_id), AirQuality::Id)) {
        ESP_LOGW(TAG, "Air Quality cluster not present on endpoint %u; skipping AQ instance init", endpoint_id);
        return ESP_ERR_INVALID_STATE;
    }

    // Create Air Quality Instance with features
    s_airQualityInstance = new AirQuality::Instance(
        chip::EndpointId(endpoint_id),
        chip::BitMask<AirQuality::Feature, uint32_t>(
            AirQuality::Feature::kFair, 
            AirQuality::Feature::kModerate,
            AirQuality::Feature::kVeryPoor,
            AirQuality::Feature::kExtremelyPoor
        )
    );
    
    if (!s_airQualityInstance) {
        ESP_LOGE(TAG, "Failed to allocate memory for Air Quality Instance");
        return ESP_FAIL;
    }
    
    CHIP_ERROR err = s_airQualityInstance->Init();
    if (err != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "Failed to initialize Air Quality Instance: %" CHIP_ERROR_FORMAT, err.Format());
        delete s_airQualityInstance;
        s_airQualityInstance = nullptr;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Air Quality Instance initialized successfully");
    return ESP_OK;
}

/**
 * @brief Update Matter attributes with current sensor values
 * 
 * @param sensor_manager Pointer to the SensorManager instance
 */
void update_matter_with_sensor_values(const SensorManager* sensor_manager) {
    static uint32_t update_count = 0;
    static const uint32_t EPAPER_UPDATE_INTERVAL = CONFIG_EINK_UPDATE_INTERVAL;
    
    if (!sensor_manager) {
        ESP_LOGE(TAG, "Invalid sensor manager pointer");
        return;
    }

    const SHT40Sensor* sht = sensor_manager->getSHT40Sensor();
    const ENS160Sensor* ens = sensor_manager->getENS160Sensor();

    float temp = 0.0f;
    float humidity = 0.0f;
    uint16_t co2 = 0;
    uint16_t tvoc = 0;
    uint16_t aqi = 0;

    if (sht && sht->validateReading()) {
        temp = sht->getTemperature();
        humidity = sht->getHumidity();
        
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

    if (ens && ens->validateReading()) {
        co2 = ens->getECO2ppm();
        tvoc = ens->getTVOCppb();
        aqi = ens->getAQI();
        
        AirQuality::AirQualityEnum airQuality = map_aqi_uba(aqi);
        ESP_LOGI(TAG, "Updating Matter AQI: %u (air quality: %d, TVOC: %u ppb, eCO2: %u ppm)",
                aqi, static_cast<uint8_t>(airQuality), tvoc, co2);
        
        if (s_airQualityInstance != nullptr) {
            // Lock the CHIP stack before calling UpdateAirQuality
            chip::DeviceLayer::StackLock lock;
            s_airQualityInstance->UpdateAirQuality(airQuality);
        } else {
            ESP_LOGE(TAG, "Air Quality Instance not initialized");
        }
    }
    
    // Update e-paper display periodically
    update_count++;
    if (update_count >= EPAPER_UPDATE_INTERVAL) {
        update_count = 0;
        ESP_LOGI(TAG, "Updating e-paper display (interval: %lu seconds)", EPAPER_UPDATE_INTERVAL);
        epaper_update_display(sensor_manager);
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