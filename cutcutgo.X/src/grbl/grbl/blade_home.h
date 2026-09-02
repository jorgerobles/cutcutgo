#ifndef __INC_GRBL_BLADE_HOME_H
#define __INC_GRBL_BLADE_HOME_H

#include <stdint.h>

uint8_t blade_home_run(void);
void blade_home_after_homing(void);
void blade_home_poll(void);
void blade_home_report(void);

#endif /* __INC_GRBL_BLADE_HOME_H */
