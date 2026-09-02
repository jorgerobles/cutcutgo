#ifndef __INC_SIM_SENSORS_SIM_H
#define __INC_SIM_SENSORS_SIM_H

#include <stdint.h>

void sim_mark_set(int32_t value_q16);
void sim_mark_timeout(uint8_t on);
void sim_mark_fault(uint8_t on);

void sim_blade_set(uint8_t detected);
void sim_blade_timeout(uint8_t on);
void sim_blade_fault(uint8_t on);
void sim_blade_unsupported(uint8_t on);

#endif /* __INC_SIM_SENSORS_SIM_H */
