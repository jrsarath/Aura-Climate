#include <esp_log.h>
#include <esp_system.h>
#include <esp_https_ota.h>
#include <esp_app_format.h>
#include "includes/ota_manager.hpp"

static const char* TAG = "ota_manager";

/**
 * @brief Initialize the OTA manager and check for any pending OTA updates.
 * 
 * @return esp_err_t 
 */
esp_err_t OTAManager::initialize() {
    const esp_app_desc_t* app_desc = esp_app_get_description();
    strncpy(current_version, app_desc->version, sizeof(current_version) - 1);
    current_version[sizeof(current_version) - 1] = '\0';

    // Check if we need to rollback
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "First boot after OTA, checking if update is working");
            bool should_rollback = false;
            
            // TODO: Add your firmware validation checks here
            // For example, check if sensors can be initialized
            // if (!validateSensors()) should_rollback = true;
            
            if (should_rollback) {
                ESP_LOGE(TAG, "Update validation failed, rolling back");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            } else {
                esp_ota_mark_app_valid_cancel_rollback();
                ESP_LOGI(TAG, "Update validated successfully");
            }
        }
    }

    ESP_LOGI(TAG, "Running firmware version: %s", current_version);
    return ESP_OK;
}

/**
 * @brief Check for available OTA updates and initiate update if found.
 * 
 */
void OTAManager::checkForUpdates() {
    if (update_in_progress) {
        ESP_LOGW(TAG, "Update already in progress");
        return;
    }

    beginUpdate(OTA_UPDATE_URL);
}

/** 
 * @brief Begin the OTA update process from the specified URL.
 * 
 */
esp_err_t OTAManager::beginUpdate(const char* url) {
    if (update_in_progress) {
        return ESP_ERR_INVALID_STATE;
    }

    update_in_progress = true;
    
    // Start update task
    BaseType_t ret = xTaskCreate(
        updateTask,
        "ota_update",
        OTA_TASK_STACK_SIZE,
        this,
        OTA_TASK_PRIORITY,
        &task_handle
    );

    if (ret != pdPASS) {
        update_in_progress = false;
        ESP_LOGE(TAG, "Failed to create OTA task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/** 
 * @brief Task function to perform the OTA update.
 * 
 */
void OTAManager::updateTask(void* pvParameter) {
    OTAManager* manager = static_cast<OTAManager*>(pvParameter);
    const char* update_url = OTA_UPDATE_URL;

    esp_http_client_config_t config = {
        .url = update_url,
        .timeout_ms = OTA_FIRMWARE_TIMEOUT_MS,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    ESP_LOGI(TAG, "Starting OTA update from %s", update_url);
    esp_err_t ret = esp_https_ota(&ota_config);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA update completed successfully");
        ESP_LOGI(TAG, "Restarting system...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ret));
    }

    manager->update_in_progress = false;
    vTaskDelete(NULL);
}

/** 
 * @brief Enable or disable automatic periodic OTA checks.
 * 
 * @param enable True to enable, false to disable.
 */
void OTAManager::enableAutoCheck(bool enable) {
    if (enable && !auto_check_enabled) {
        startAutoCheckTask();
    } else if (!enable && auto_check_enabled) {
        stopAutoCheckTask();
    }
    auto_check_enabled = enable;
}

/** 
 * @brief Start the periodic OTA check task.
 * 
 */
void OTAManager::startAutoCheckTask() {
    xTaskCreate(
        [](void* pvParameter) {
            OTAManager* manager = static_cast<OTAManager*>(pvParameter);
            TickType_t last_check = xTaskGetTickCount();
            
            while (manager->auto_check_enabled) {
                manager->checkForUpdates();
                vTaskDelayUntil(&last_check, pdMS_TO_TICKS(OTA_CHECK_INTERVAL_MS));
            }
            vTaskDelete(NULL);
        },
        "ota_check",
        OTA_TASK_STACK_SIZE,
        this,
        OTA_TASK_PRIORITY,
        &task_handle
    );
}

/** 
 * @brief Stop the periodic OTA check task.
 * 
 */
void OTAManager::stopAutoCheckTask() {
    if (task_handle) {
        vTaskDelete(task_handle);
        task_handle = nullptr;
    }
} 