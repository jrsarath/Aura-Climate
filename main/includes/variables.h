#pragma once

#include <sgp40.h>
#include <inttypes.h>

#define VARIABLES_H

#ifdef __cplusplus
extern "C" {
#endif

extern sgp40_t sgp;

extern float temperature, humidity;
extern int32_t voc_index;

extern uint16_t temperature_endpoint_id;
extern uint16_t humidity_endpoint_id;
extern uint16_t voc_endpoint_id;

// default values
#define DEFAULT_TEMPERATURE_VALUE 1000
#define DEFAULT_HUMIDITY_VALUE 1000
#define DEFAULT_VOC_VALUE 1000

#ifdef __cplusplus
}
#endif