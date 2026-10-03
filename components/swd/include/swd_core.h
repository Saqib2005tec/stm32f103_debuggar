#ifndef SWD_CORE_H
#define SWD_CORE_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Cortex-M Debug Registers
 */
#define CORE_DEBUG_BASE     0xE000EDF0UL

#define DHCSR               0xE000EDF0UL
#define DCRSR               0xE000EDF4UL
#define DCRDR               0xE000EDF8UL
#define DEMCR               0xE000EDFCUL

/*
 * Debug Fault Status Register
 */
#define DFSR                0xE000ED30UL
#define DFSR_HALTED         (1UL << 0)
#define DFSR_BKPT           (1UL << 1)
#define DFSR_DWTTRAP        (1UL << 2)
#define DFSR_VCATCH         (1UL << 3)
#define DFSR_EXTERNAL       (1UL << 4)

/*
 * Flash Patch and Breakpoint (FPB) Registers
 */
#define FPB_BASE            0xE0002000UL
#define FP_CTRL             0xE0002000UL
#define FP_REMAP            0xE0002004UL
#define FP_COMP0            0xE0002008UL
#define FP_COMP(n)          (FP_COMP0 + ((uint32_t)(n) * 4UL))

#define FP_CTRL_KEY         (1UL << 1)
#define FP_CTRL_ENABLE      (1UL << 0)

#define FP_MAX_BREAKPOINTS  6

/*
 * DHCSR Debug Key
 */
#define DHCSR_DBGKEY        0xA05FUL

/*
 * DHCSR Control bits
 */
#define DHCSR_C_DEBUGEN     (1UL << 0)
#define DHCSR_C_HALT        (1UL << 1)
#define DHCSR_C_STEP        (1UL << 2)
#define DHCSR_C_MASKINTS    (1UL << 3)

/*
 * DHCSR Status bits
 */
#define DHCSR_S_REGRDY      (1UL << 16)
#define DHCSR_S_HALT        (1UL << 17)
#define DHCSR_S_SLEEP       (1UL << 18)
#define DHCSR_S_LOCKUP      (1UL << 19)
#define DHCSR_S_RETIRE_ST   (1UL << 24)
#define DHCSR_S_RESET_ST    (1UL << 25)

/*
 * Cortex-M CPU register selectors
 */
#define CORE_REG_R0         0
#define CORE_REG_R1         1
#define CORE_REG_R2         2
#define CORE_REG_R3         3
#define CORE_REG_R4         4
#define CORE_REG_R5         5
#define CORE_REG_R6         6
#define CORE_REG_R7         7
#define CORE_REG_R8         8
#define CORE_REG_R9         9
#define CORE_REG_R10        10
#define CORE_REG_R11        11
#define CORE_REG_R12        12
#define CORE_REG_SP         13
#define CORE_REG_LR         14
#define CORE_REG_PC         15
#define CORE_REG_XPSR       16

/*
 * Core control functions
 */
bool core_debug_enable(void);
bool core_halt(void);
bool core_resume(void);
bool core_step(void);
bool core_is_halted(void);
uint32_t core_read_dhcsr(void);

/*
 * CPU register access
 */
bool core_read_register(uint8_t reg_num, uint32_t *value);
void core_dump_registers(void);

/*
 * Hardware Breakpoint API (FPB)
 */
bool core_bp_enable_unit(bool enable);
bool core_bp_set(uint8_t index, uint32_t address);
bool core_bp_clear(uint8_t index);
bool core_bp_clear_all(void);
bool core_bp_get(uint8_t index, bool *enabled, uint32_t *address);
int8_t core_bp_find_free_slot(void);
bool core_bp_is_active_at(uint32_t address);
bool core_is_breakpoint_hit(void);
bool core_resume_with_stepover(void);
bool core_step_with_stepover(void);

#endif
