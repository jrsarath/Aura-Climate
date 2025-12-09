#pragma once

// System Configuration
#define SYSTEM_TASK_STACK_SIZE     4096
#define SYSTEM_TASK_PRIORITY       5
#define SYSTEM_WATCHDOG_TIMEOUT_MS 5000

// Sensor Configuration
#define SENSOR_READ_INTERVAL_MS    10000 // 60000 ms = 1 minutes
#define MATTER_UPDATE_INTERVAL_MS  10000 // 30000 ms = 30 seconds
#define SENSOR_MAX_RETRIES         3
#define SENSOR_RETRY_DELAY_MS      1000

// SHT4x Sensor Configuration
#define SHT4X_ERROR_RETRY_COUNT    3
#define SHT4X_MIN_TEMPERATURE      -40.0f
#define SHT4X_MAX_TEMPERATURE      125.0f
#define SHT4X_MIN_HUMIDITY         0.0f
#define SHT4X_MAX_HUMIDITY         100.0f
#define SHT4X_HEATER_MODE          SHT4X_HEATER_OFF
#define SHT4X_REPEATABILITY        SHT4X_HIGH

// ENS160 Sensor Configuration
#define ENS160_I2C_ADDR            0x53
#define ENS160_ERROR_RETRY_COUNT   3
#define ENS160_MIN_AQI             1
#define ENS160_MAX_AQI             5
#define ENS160_MIN_TVOC_PPB        0
#define ENS160_MAX_TVOC_PPB        65000
#define ENS160_MIN_ECO2_PPM        400
#define ENS160_MAX_ECO2_PPM        65000
#define ENS160_DATA_POLL_TIMEOUT_MS 1500

// OTA Configuration
#define OTA_TASK_STACK_SIZE        8192
#define OTA_TASK_PRIORITY         5
#define OTA_CHECK_INTERVAL_MS     3600000  // Check for updates every hour
#define OTA_FIRMWARE_TIMEOUT_MS   300000   // 5 minutes timeout for firmware download
#define OTA_MAX_RETRIES          3
#define OTA_RETRY_DELAY_MS       5000
#define OTA_BUFFER_SIZE          1024
#define OTA_ROLLBACK_ENABLED     1         // Enable rollback on failed boot
#define OTA_UPDATE_URL           "https://ota.48studios.dev/aura/climate/2"

// Logging Configuration
#define LOG_BUFFER_SIZE            1024
#define MAX_LOG_FILES              5
#define MAX_LOG_FILE_SIZE          (1024 * 1024) // 1MB

// Matter Configuration
#define MATTER_MAX_ENDPOINTS       16
#define COMMISSIONING_TIMEOUT_SEC  300 

// SNTP CONFIG
#define SNTP_SERVER1               "pool.ntp.org"
#define SNTP_SERVER2               "time.nist.gov"
#define SNTP_SERVER3               "time.google.com"
#define SNTP_STARTUP_DELAY_MS      2000  // Delay before starting SNTP after IP available
#define SNTP_DEFAULT_TZ           "UTC-05:30"  // Asia/Kolkata (IST)
// Timezone Auto-Detection Configuration
#define TIMEZONE_API_URL           "http://worldtimeapi.org/api/ip"
#define TIMEZONE_API_TIMEOUT_MS    5000