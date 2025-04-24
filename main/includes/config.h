#pragma once

// System Configuration
#define SYSTEM_TASK_STACK_SIZE     4096
#define SYSTEM_TASK_PRIORITY       5
#define SYSTEM_WATCHDOG_TIMEOUT_MS 5000

// Sensor Configuration
#define SENSOR_READ_INTERVAL_MS    2000 
#define MATTER_UPDATE_INTERVAL_MS  30000
#define SENSOR_MAX_RETRIES         3
#define SENSOR_RETRY_DELAY_MS      1000

// DHT Sensor Configuration
#define DHT_TYPE                   DHT_TYPE_AM2301
#define DHT_ERROR_RETRY_COUNT      3
#define DHT_MIN_TEMPERATURE        -40.0f
#define DHT_MAX_TEMPERATURE        80.0f
#define DHT_MIN_HUMIDITY           0.0f
#define DHT_MAX_HUMIDITY           100.0f

// SGP40 Sensor Configuration
#define SGP40_I2C_ADDR             0x59
#define SGP40_ERROR_RETRY_COUNT    3
#define SGP40_MIN_VOC_INDEX        0
#define SGP40_MAX_VOC_INDEX        500

// Logging Configuration
#define LOG_BUFFER_SIZE            1024
#define MAX_LOG_FILES              5
#define MAX_LOG_FILE_SIZE          (1024 * 1024) // 1MB

// Matter Configuration
#define MATTER_MAX_ENDPOINTS       16
#define COMMISSIONING_TIMEOUT_SEC  300 