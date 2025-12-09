#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <esp_err.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config.hpp"
#include "time_manager.hpp"


static const char *TAG = "TIME MGR";
static bool sntp_started = false;
static bool tz_auto_set = false;
static TaskHandle_t tz_task_handle = nullptr;

static void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "Time synchronized via SNTP");
}

// Extracts "utc_offset":"+05:30" from a small JSON response.
static bool extract_utc_offset(const char *body, char *offset_out, size_t out_len) {
    if (!body || !offset_out || out_len < 7) {  // needs space for "+HH:MM" and null
        return false;
    }
    const char *key = "\"utc_offset\":\"";
    const char *p = strstr(body, key);
    if (!p) {
        return false;
    }
    p += strlen(key);
    const char *end = strchr(p, '"');
    if (!end) {
        return false;
    }
    size_t len = (size_t)(end - p);
    if (len >= out_len) {
        return false;
    }
    memcpy(offset_out, p, len);
    offset_out[len] = '\0';
    return true;
}

// Convert "+05:30" to POSIX TZ string (e.g., "UTC-05:30"). POSIX sign is reversed.
static bool utc_offset_to_posix(const char *utc_offset, char *tz_out, size_t tz_len) {
    if (!utc_offset || strlen(utc_offset) < 3) {
        return false;
    }
    char sign;
    int hh = 0, mm = 0;
    if (sscanf(utc_offset, "%c%2d:%2d", &sign, &hh, &mm) != 3) {
        return false;
    }
    if ((sign != '+' && sign != '-') || hh < 0 || hh > 14 || mm < 0 || mm >= 60) {
        return false;
    }
    char posix_sign = (sign == '+') ? '-' : '+';  // POSIX TZ sign is inverted
    snprintf(tz_out, tz_len, "UTC%c%02d:%02d", posix_sign, hh, mm);
    return true;
}

static esp_err_t fetch_and_apply_timezone(void) {
    const char *url = TIMEZONE_API_URL;
    char resp[512] = {0};

    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = TIMEZONE_API_TIMEOUT_MS,
        .buffer_size = sizeof(resp) - 1,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGW(TAG, "Failed to init HTTP client for timezone fetch");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int read_len = esp_http_client_read(client, resp, sizeof(resp) - 1);
    resp[(read_len > 0) ? read_len : 0] = '\0';
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (read_len <= 0) {
        ESP_LOGW(TAG, "No data from timezone service");
        return ESP_FAIL;
    }

    char utc_offset[8] = {0};
    if (!extract_utc_offset(resp, utc_offset, sizeof(utc_offset))) {
        ESP_LOGW(TAG, "Failed to parse utc_offset from response");
        return ESP_FAIL;
    }

    char tz[16] = {0};
    if (!utc_offset_to_posix(utc_offset, tz, sizeof(tz))) {
        ESP_LOGW(TAG, "Failed to convert utc_offset %s to POSIX TZ", utc_offset);
        return ESP_FAIL;
    }

    setenv("TZ", tz, 1);
    tzset();
    ESP_LOGI(TAG, "Timezone auto-set to %s (from utc_offset %s)", tz, utc_offset);
    return ESP_OK;
}

static void timezone_auto_task(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(SNTP_STARTUP_DELAY_MS));

    if (fetch_and_apply_timezone() == ESP_OK) {
        tz_auto_set = true;
    }

    tz_task_handle = nullptr;
    vTaskDelete(NULL);
}

static void start_sntp_if_needed(void) {
    if (sntp_started) {
        return;
    }

    ESP_LOGI(TAG, "Starting SNTP time sync");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, SNTP_SERVER1);
    sntp_setservername(1, SNTP_SERVER2);
    sntp_setservername(2, SNTP_SERVER3);
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();

    // Default to Asia/Kolkata (IST) unless auto-detected or configured elsewhere.
    setenv("TZ", SNTP_DEFAULT_TZ, 1);
    tzset();

    sntp_started = true;
}

static void start_timezone_auto_task_if_needed(void) {
    if (tz_auto_set || tz_task_handle != nullptr) {
        return;
    }

    BaseType_t ret = xTaskCreate(timezone_auto_task, "tz_auto", 4096, NULL, 4, &tz_task_handle);
    if (ret != pdPASS) {
        ESP_LOGW(TAG, "Failed to start timezone auto task");
        tz_task_handle = nullptr;
    }
}

void time_manager_handle_ip_available(void) {
    start_sntp_if_needed();
    start_timezone_auto_task_if_needed();
}
