#ifndef SWD_AP_H
#define SWD_AP_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * AHB-AP Registers
 * ============================================================ */

#define AP_REG_CSW     0x00
#define AP_REG_TAR     0x04
#define AP_REG_DRW     0x0C
#define AP_REG_BASE    0xF8
#define AP_REG_IDR     0xFC


/* ============================================================
 * AHB-AP CSW Configuration
 * ============================================================ */

#define AP_CSW_SIZE_32BIT       (2U << 0)
#define AP_CSW_ADDRINC_SINGLE   (1U << 4)
#define AP_CSW_DEVICE_EN        (1U << 6)
#define AP_CSW_HPROT            ((1U << 29) | (3U << 24))

#define AP_CSW_32BIT_SINGLE \
    (AP_CSW_SIZE_32BIT | \
     AP_CSW_ADDRINC_SINGLE | \
     AP_CSW_DEVICE_EN | \
     AP_CSW_HPROT)


/* ============================================================
 * AP Selection
 * ============================================================ */

#define AP_BANK_0       0x0
#define AP_BANK_F       0xF
#define AP_AHB_APSEL    0


/* ============================================================
 * CSW Fields
 * ============================================================ */

#define AP_CSW_SIZE_MASK        0x7UL
#define AP_CSW_ADDRINC_MASK     (3UL << 4)

#define AP_CSW_SIZE_BYTE        0x0UL
#define AP_CSW_SIZE_HALFWORD    0x1UL
#define AP_CSW_SIZE_WORD        0x2UL


/* ============================================================
 * Access Size
 * ============================================================ */

#define AP_ACCESS_8BIT          0x0U
#define AP_ACCESS_16BIT         0x1U
#define AP_ACCESS_32BIT         0x2U


/* ============================================================
 * Generic Access Type
 * ============================================================ */

typedef uint32_t ap_access_size_t;


/* ============================================================
 * Basic AP Access
 * ============================================================ */

uint32_t ap_read(uint8_t addr);

bool ap_write(
    uint8_t addr,
    uint32_t data
);


/* ============================================================
 * CSW Configuration
 * ============================================================ */

bool ap_configure_32bit(void);

bool ap_set_access_size(
    ap_access_size_t size
);

bool ap_verify_csw(void);


/* ============================================================
 * Identification
 * ============================================================ */

uint32_t ap_read_idr(void);
uint32_t ap_read_base(void);


/* ============================================================
 * Explicit Memory Access
 * ============================================================ */

bool ap_memory_read32(
    uint32_t address,
    uint32_t *data
);

bool ap_memory_read16(
    uint32_t address,
    uint16_t *data
);

bool ap_memory_write32(
    uint32_t address,
    uint32_t data
);

bool ap_memory_write16(
    uint32_t address,
    uint16_t data
);


/* ============================================================
 * Generic Memory Access
 * ============================================================ */

bool ap_memory_read(
    uint32_t address,
    uint32_t *data,
    ap_access_size_t size
);

bool ap_memory_write(
    uint32_t address,
    uint32_t data,
    ap_access_size_t size
);

#endif