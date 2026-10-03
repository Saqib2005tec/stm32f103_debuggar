#ifndef SWD_DEBUG_H
#define SWD_DEBUG_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    TARGET_DISCONNECTED = 0,
    TARGET_CONNECTED,
    TARGET_RUNNING,
    TARGET_HALTED

} target_state_t;


bool debug_connect(void);

bool debug_disconnect(void);

target_state_t debug_get_state(void);

uint32_t debug_get_dpidr(void);

uint32_t debug_get_ap_idr(void);

uint32_t debug_get_ap_base(void);

bool debug_reset_target(void);

bool swd_test_programmed_firmware_execution(void);

#endif