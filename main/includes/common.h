#pragma once

#include <stdlib.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define DEFAULT_TEMPERATURE_VALUE 1000
#define DEFAULT_HUMIDITY_VALUE 1000
#define DEFAULT_VOC_VALUE 1000

#define ABORT_APP_ON_FAILURE(x, ...) do {           \
        if (!(unlikely(x))) {                       \
            __VA_ARGS__;                            \
            vTaskDelay(5000 / portTICK_PERIOD_MS);  \
            abort();                                \
        }                                           \
    } while (0)