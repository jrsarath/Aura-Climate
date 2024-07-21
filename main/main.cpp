
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
#include <app/server/Server.h>
#include <app/server/CommissioningWindowManager.h>

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <platform/ESP32/OpenthreadLauncher.h>
#endif

#include "includes/common.h"
#include "includes/driver.h"
#include "includes/sensors.h"
#include "includes/variables.h"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static const char *TAG = "matter";
constexpr auto k_timeout_seconds = 300;
uint16_t temperature_endpoint_id = 1;
uint16_t humidity_endpoint_id = 2;
uint16_t voc_endpoint_id = 3;

// CONFIG_ENABLE_ENCRYPTED_OTA
#if CONFIG_ENABLE_ENCRYPTED_OTA
extern const char decryption_key_start[] asm("_binary_esp_image_encryption_key_pem_start");
extern const char decryption_key_end[] asm("_binary_esp_image_encryption_key_pem_end");

static const char *s_decryption_key = decryption_key_start;
static const uint16_t s_decryption_key_len = decryption_key_end - decryption_key_start;
#endif

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg) {
    switch (event->Type) {
        case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
            ESP_LOGI(TAG, "Interface IP Address changed");
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
        case chip::DeviceLayer::DeviceEventType::kFabricRemoved: {
                ESP_LOGI(TAG, "Fabric removed successfully");
                if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0) {
                    chip::CommissioningWindowManager & commissionMgr = chip::Server::GetInstance().GetCommissioningWindowManager();
                    constexpr auto kTimeoutSeconds = chip::System::Clock::Seconds16(k_timeout_seconds);
                    if (!commissionMgr.IsCommissioningWindowOpen()) {
                        /* After removing last fabric, this example does not remove the Wi-Fi credentials
                        * and still has IP connectivity so, only advertising on DNS-SD.
                        */
                        CHIP_ERROR err = commissionMgr.OpenBasicCommissioningWindow(kTimeoutSeconds, chip::CommissioningWindowAdvertisement::kDnssdOnly);
                        if (err != CHIP_NO_ERROR) {
                            ESP_LOGE(TAG, "Failed to open commissioning window, err:%" CHIP_ERROR_FORMAT, err.Format());
                        }
                    }
                }
            break;
        }
        case chip::DeviceLayer::DeviceEventType::kFabricWillBeRemoved:
            ESP_LOGI(TAG, "Fabric will be removed");
            break;
        case chip::DeviceLayer::DeviceEventType::kFabricUpdated:
            ESP_LOGI(TAG, "Fabric is updated");
            break;
        case chip::DeviceLayer::DeviceEventType::kFabricCommitted:
            ESP_LOGI(TAG, "Fabric is committed");
            break;
        case chip::DeviceLayer::DeviceEventType::kBLEDeinitialized:
            ESP_LOGI(TAG, "BLE deinitialized and memory reclaimed");
            break;
        default:
            break;
    }
}
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id, uint8_t effect_variant, void *priv_data) {
    ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
    return ESP_OK;
}
static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data) {
    esp_err_t err = ESP_OK;
    return err;
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

    // Start Matter stack
    err = esp_matter::start(app_event_cb);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to start Matter, err:%d", err));

    #if CONFIG_ENABLE_ENCRYPTED_OTA
        // Initialize encrypted OTA updates
        err = esp_matter_ota_requestor_encrypted_init(s_decryption_key, s_decryption_key_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialized the encrypted OTA, err: %d", err);
            abort();
        }
    #endif // CONFIG_ENABLE_ENCRYPTED_OTA

    #if CONFIG_ENABLE_CHIP_SHELL
        esp_matter::console::diagnostics_register_commands();
        esp_matter::console::wifi_register_commands();
        #if CONFIG_OPENTHREAD_CLI
            esp_matter::console::otcli_register_commands();
        #endif
        esp_matter::console::init();
    #endif
}