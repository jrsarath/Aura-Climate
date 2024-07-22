#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_system.h>
#include <esp_matter.h>
#include <esp_matter_ota.h>
#include <esp_matter_console.h>
#include <app_reset.h>
#include <common_macros.h>
#include <app/server/Server.h>
#include <app/server/CommissioningWindowManager.h>

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <platform/ESP32/OpenthreadLauncher.h>
#endif

#include "includes/driver.h"
#include "includes/sensors.h"
#include "includes/variables.h"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static const char *TAG = "matter";
constexpr auto k_timeout_seconds = 300;

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg){
    switch (event->Type) {
        case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
            ESP_LOGI(TAG, "Interface IP Address Changed");
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
            ESP_LOGI(TAG, "Commissioning complete");
            break;

        case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
            ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
            ESP_LOGI(TAG, "Commissioning session started");
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
            ESP_LOGI(TAG, "Commissioning session stopped");
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
            ESP_LOGI(TAG, "Commissioning window opened");
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
            ESP_LOGI(TAG, "Commissioning window closed");
            break;

        default:
            break;
    }
}
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id, uint8_t effect_variant, void *priv_data){
    ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
    return ESP_OK;
}
static esp_err_t app_attribute_update_cb(callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data){
    if (type == PRE_UPDATE) {
        /* Handle the attribute updates here. */
    }
    return ESP_OK;
}

extern "C" void app_main() {
    esp_err_t err = ESP_OK;
    nvs_flash_init();
    driver_handle dht_handle = driver_dht_init();
    driver_handle sgp_handle = driver_voc_init();
    driver_handle button_handle = driver_button_init();
    app_reset_button_register(button_handle);

    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    ABORT_APP_ON_FAILURE(node != nullptr, ESP_LOGE(TAG, "Failed to create Matter node"));

    // Temperature Sensor Configuration
    temperature_sensor::config_t temperature_config;
    temperature_config.temperature_measurement.measured_value = DEFAULT_TEMPERATURE_VALUE;
    endpoint_t *temperature_endpoint = temperature_sensor::create(node, &temperature_config, ENDPOINT_FLAG_NONE, dht_handle);
    ABORT_APP_ON_FAILURE(temperature_endpoint != nullptr, ESP_LOGE(TAG, "Failed to create temperature endpoint"));
    temperature_endpoint_id = endpoint::get_id(temperature_endpoint);
    ESP_LOGI(TAG, "Temperature created with endpoint_id %d", temperature_endpoint_id);
    esp_matter::attribute::set_callback(sensor_attribute_update_cb);

    // Humidity Sensor Configuration
    humidity_sensor::config_t humidity_config;
    humidity_config.relative_humidity_measurement.measured_value = DEFAULT_HUMIDITY_VALUE;
    endpoint_t *humidity_endpoint = humidity_sensor::create(node, &humidity_config, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(humidity_endpoint != nullptr, ESP_LOGE(TAG, "Failed to create humidty endpoint"));
    humidity_endpoint_id = endpoint::get_id(humidity_endpoint);
    ESP_LOGI(TAG, "Humidity created with endpoint_id %d", humidity_endpoint_id);
    esp_matter::attribute::set_callback(sensor_attribute_update_cb);

    // Air Quality Sensor Configuration
    air_quality_sensor::config_t voc_config;
    endpoint_t *voc_endpoint = air_quality_sensor::create(node, &voc_config, ENDPOINT_FLAG_NONE, sgp_handle);
    ABORT_APP_ON_FAILURE(voc_endpoint != nullptr, ESP_LOGE(TAG, "Failed to create air quality endpoint"));
    voc_endpoint_id = endpoint::get_id(voc_endpoint);
    ESP_LOGI(TAG, "AQI created with endpoint_id %d", voc_endpoint_id);
    esp_matter::attribute::set_callback(sensor_attribute_update_cb);

    #if CHIP_DEVICE_CONFIG_ENABLE_THREAD
        /* Set OpenThread platform config */
        esp_openthread_platform_config_t config = {
            .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
            .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
            .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
        };
        set_openthread_platform_config(&config);
    #endif

    /* Matter start */
    err = esp_matter::start(app_event_cb);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to start Matter, err:%d", err));

    #if CONFIG_ENABLE_CHIP_SHELL
        esp_matter::console::diagnostics_register_commands();
        esp_matter::console::wifi_register_commands();
        esp_matter::console::init();
    #endif
}