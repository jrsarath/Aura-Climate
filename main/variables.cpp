#include "includes/variables.hpp"

// Matter endpoint IDs
uint16_t temperature_endpoint_id = 1;
uint16_t humidity_endpoint_id = 2;
uint16_t air_quality_endpoint_id = 3;

// SGP40 sensor timing
const uint32_t SGP40_WARMUP_TIME_MS = 10000;  // 10 seconds minimum warmup
uint32_t sgp40_start_time_ms = 0;