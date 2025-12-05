#pragma once

#include <inttypes.h>

// Matter endpoint IDs
extern uint16_t temperature_endpoint_id;
extern uint16_t humidity_endpoint_id;
extern uint16_t voc_endpoint_id;

// SGP40 sensor timing
extern const uint32_t SGP40_WARMUP_TIME_MS;  // Warmup time in milliseconds
extern uint32_t sgp40_start_time_ms;         // Start time timestamp