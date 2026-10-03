#ifndef SWD_DP_H
#define SWD_DP_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Debug Port register offsets
 *
 * These are byte offsets inside the DP register interface.
 */
#define DP_REG_DPIDR       0x00
#define DP_REG_ABORT       0x00

#define DP_REG_CTRL_STAT   0x04

#define DP_REG_SELECT      0x08

#define DP_REG_RDBUFF      0x0C


/*
 * CTRL/STAT register
 */

/*
 * Power request
 */
#define DP_CTRL_CDBGPWRUPREQ    (1UL << 28)
#define DP_CTRL_CDBGPWRUPACK    (1UL << 29)

#define DP_CTRL_CSYSPWRUPREQ    (1UL << 30)
#define DP_CTRL_CSYSPWRUPACK    (1UL << 31)

/*
 * Sticky error/status
 */
#define DP_CTRL_STICKYERR       (1UL << 5)
#define DP_CTRL_STICKYCMP       (1UL << 4)
#define DP_CTRL_STICKYORUN      (1UL << 1)


#define DP_POWER_UP_MASK \
    (DP_CTRL_CDBGPWRUPREQ | \
     DP_CTRL_CSYSPWRUPREQ)

#define DP_POWER_ACK_MASK \
    (DP_CTRL_CDBGPWRUPACK | \
     DP_CTRL_CSYSPWRUPACK)




/*
 * DP ABORT register
 */

#define DP_ABORT_DAPABORT       (1UL << 0)
#define DP_ABORT_STKCMPCLR      (1UL << 1)
#define DP_ABORT_STKERRCLR      (1UL << 2)
#define DP_ABORT_WDERRCLR       (1UL << 3)
#define DP_ABORT_ORUNERRCLR     (1UL << 4)

/*
 * DP operations
 */

/*
 * Read a DP register.
 */
uint32_t dp_read(uint8_t addr);

/*
 * Write a DP register.
 */
bool dp_write(uint8_t addr, uint32_t data);

/*
 * Read DPIDR.
 */
uint32_t dp_read_idcode(void);

/*
 * Request debug and system power-up.
 */
bool dp_power_up(void);

/*
 * Read CTRL/STAT.
 */
uint32_t dp_read_ctrl_stat(void);

/*
 * Select AP and AP bank.
 */
bool dp_select(uint8_t apsel, uint8_t apbanksel);

/*
 * Read RDBUFF.
 */
uint32_t dp_read_rdbuff(void);

bool dp_clear_errors(void);
void dp_print_ctrl_stat(
    uint32_t value
);

#endif