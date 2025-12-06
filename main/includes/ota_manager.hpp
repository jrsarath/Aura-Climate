#pragma once

#include <esp_err.h>
#include <esp_ota_ops.h>
#include <esp_http_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config.hpp"

/**
 * @brief Manages Over-The-Air (OTA) updates for the device.
 * 
 */
class OTAManager {
    public:
        /**
         * @brief Get the singleton instance of the OTAManager.
         * 
         * @return OTAManager& Reference to the OTAManager instance.
         */
        static OTAManager& getInstance() {
            static OTAManager instance;
            return instance;
        }

        esp_err_t initialize();
        esp_err_t beginUpdate(const char* url);
        void checkForUpdates();
        void enableAutoCheck(bool enable);
        const char* getCurrentVersion() const { return current_version; }
        bool isUpdateInProgress() const { return update_in_progress; }
        bool isNetworkReady() const { return network_ready; }
        void setNetworkReady(bool ready) { network_ready = ready; }
        void setUpdateCallback(void (*callback)(int progress)) { progress_callback = callback; }

    private:
        OTAManager() : update_in_progress(false), auto_check_enabled(false), network_ready(false), 
                    progress_callback(nullptr), task_handle(nullptr) {}
        ~OTAManager() = default;
        OTAManager(const OTAManager&) = delete;
        OTAManager& operator=(const OTAManager&) = delete;

        static void updateTask(void* pvParameter);
        esp_err_t performUpdate(const char* url);
        esp_err_t validateUpdate();
        void markUpdateSuccessful();
        void startAutoCheckTask();
        void stopAutoCheckTask();

        bool update_in_progress;
        bool auto_check_enabled;
        bool network_ready;
        char current_version[32];
        void (*progress_callback)(int progress);
        TaskHandle_t task_handle;
        esp_ota_handle_t ota_handle;
}; 