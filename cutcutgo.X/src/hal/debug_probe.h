#ifndef __INC_HAL_DEBUG_PROBE_H
#define __INC_HAL_DEBUG_PROBE_H

#include <stdint.h>

uint8_t debug_probe_execute(const char *line);

#ifdef SENSOR_BENCH
void debug_bench_task(void);
#endif

#endif /* __INC_HAL_DEBUG_PROBE_H */
