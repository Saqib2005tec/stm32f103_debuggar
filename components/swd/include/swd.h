#ifndef SWD_H
#define SWD_H

#include <stdint.h>
#include <stdbool.h>

#include "driver/spi_master.h"


#define AP_CSW      0x00
#define AP_TAR      0x04
#define AP_DRW      0x0C

/*
 * SWD physical pins
 */
#define SWCLK_GPIO  GPIO_NUM_18
#define SWDIO_GPIO  GPIO_NUM_19
#define NRST_GPIO   GPIO_NUM_21

/*
 * SWD ACK values
 */
#define SWD_ACK_OK      0x01
#define SWD_ACK_WAIT    0x02
#define SWD_ACK_FAULT   0x04
#define SWD_ACK_INVALID 0x00
#define SWD_MAX_RETRIES  100

/* AHB-AP CSW fields */

#define AP_CSW_SIZE_MASK        0x7UL

#define AP_CSW_SIZE_BYTE        0x0UL
#define AP_CSW_SIZE_HALFWORD    0x1UL
#define AP_CSW_SIZE_WORD        0x2UL

#define AP_CSW_ADDRINC_MASK     (3UL << 4)
#define AP_CSW_ADDRINC_OFF      (0UL << 4)

#define AP_CSW_ADDRINC_PACKED   (2UL << 4)

/* Access sizes */

#define AP_ACCESS_8BIT          0x0U
#define AP_ACCESS_16BIT         0x1U
#define AP_ACCESS_32BIT         0x2U
/*
 * Initialize ESP32 SWD transport
 */
void swd_init(void);

/*
 * Switch target from JTAG to SWD.
 */
void swd_jtag_to_swd(void);

/*
 * Generate an SWD request header.
 *
 * ap:
 *   false = Debug Port
 *   true  = Access Port
 *
 * read:
 *   false = write
 *   true  = read
 *
 * addr:
 *   Register byte address.
 *   Valid values are 0x00, 0x04, 0x08, 0x0C.
 */
uint8_t swd_make_request(bool ap, bool read, uint8_t addr);

/*
 * Perform a generic SWD read transaction.
 *
 * Returns the 32-bit data if ACK == OK.
 *
 * ack_out receives the target ACK.
 */
uint32_t swd_read(bool ap, uint8_t addr, uint8_t *ack_out);

/*
 * Perform a generic SWD write transaction.
 *
 * Returns true if ACK == OK.
 */
bool swd_write(bool ap, uint8_t addr, uint32_t data);

/*
 * Read target DP IDCODE.
 */
uint32_t swd_read_idcode(void);



 

typedef enum
{
    SWD_OK = 0,

    SWD_ERR_INVALID_ARG,
    SWD_ERR_TIMEOUT,

    SWD_ERR_ACK_WAIT,
    SWD_ERR_ACK_FAULT,
    SWD_ERR_ACK_INVALID,

    SWD_ERR_PARITY,

    SWD_ERR_DP_STICKY_ERROR,
    SWD_ERR_AP_FAULT,

    SWD_ERR_TARGET_NOT_HALTED,
    SWD_ERR_REGISTER_NOT_READY

} swd_status_t;

swd_status_t swd_get_last_status(void);

const char *swd_status_string(
    swd_status_t status
);

#endif