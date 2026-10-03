#include "swd_ap.h"
#include "swd.h"
#include "swd_dp.h"

#include <stdbool.h>
#include <stdio.h>
#include "swd_log.h"


bool ap_configure_32bit(void)
{
    return ap_set_access_size(
        AP_ACCESS_32BIT
    );
}

bool ap_memory_write32(
    uint32_t address,
    uint32_t data
)
{
    if (address & 0x3U)
    {
        SWD_LOG(
            "AP WRITE32 ERROR: "
            "Unaligned address 0x%08lX\n",
            (unsigned long)address
        );

        return false;
    }

    if (!ap_set_access_size(
            AP_ACCESS_32BIT))
    {
        return false;
    }

    if (!ap_write(
            AP_REG_TAR,
            address))
    {
        return false;
    }

    if (!ap_write(
            AP_REG_DRW,
            data))
    {
        return false;
    }

    return true;
}

uint32_t ap_read(uint8_t addr)
{
    uint8_t ack;

    /*
     * Start AP read.
     *
     * APnDP = 1
     * RnW   = 1
     */
    (void)swd_read(
        true,
        addr,
        &ack
    );

    /*
     * AP reads are posted.
     *
     * The actual result is retrieved
     * from DP RDBUFF.
     */
    if (ack != SWD_ACK_OK) {
        return 0;
    }

    return dp_read_rdbuff();
}


bool ap_write(
    uint8_t addr,
    uint32_t data
)
{
    /*
     * APnDP = 1
     */
    return swd_write(
        true,
        addr,
        data
    );
}

bool ap_verify_csw(void)
{
    uint32_t csw =
        ap_read(AP_REG_CSW);

    if (csw == 0)
    {
        SWD_LOG(
            "Failed to read AP CSW.\n"
        );

        return false;
    }

    SWD_LOG(
        "AP CSW = 0x%08lX\n",
        (unsigned long)csw
    );

    if ((csw & AP_CSW_SIZE_32BIT) !=
        AP_CSW_SIZE_32BIT)
    {
        SWD_LOG(
            "ERROR: AP is not configured for 32-bit access.\n"
        );

        return false;
    }

    /*
     * Verify the HPROT configuration that
     * we discovered was necessary for this target.
     */

    if ((csw & (1UL << 29)) == 0)
    {
        SWD_LOG(
            "ERROR: HPROT bit 29 is not set.\n"
        );

        return false;
    }

    return true;
}

uint32_t ap_read_idr(void)
{
    return ap_read(AP_REG_IDR);
}

uint32_t ap_read_base(void)
{
    return ap_read(AP_REG_BASE);
}

bool ap_memory_write16(
    uint32_t address,
    uint16_t data
)
{
    if (address & 0x1U)
    {
        SWD_LOG(
            "AP WRITE16 ERROR: "
            "Unaligned address 0x%08lX\n",
            (unsigned long)address
        );

        return false;
    }

    if (!ap_set_access_size(
            AP_ACCESS_16BIT))
    {
        return false;
    }

    /*
     * Keep the exact half-word address in TAR.
     * Shift data to the upper half-word lane if writing to offset 2.
     */
    uint32_t shifted_data = (address & 0x2U) ? ((uint32_t)data << 16) : (uint32_t)data;

    if (!ap_write(
            AP_REG_TAR,
            address))
    {
        SWD_LOG(
            "AP WRITE16 ERROR: "
            "Failed to write TAR\n"
        );

        return false;
    }

    if (!ap_write(
            AP_REG_DRW,
            shifted_data))
    {
        SWD_LOG(
            "AP WRITE16 ERROR: "
            "Failed to write DRW\n"
        );

        return false;
    }

    return true;
}

bool ap_memory_read16(
    uint32_t address,
    uint16_t *data
)
{
    if (data == NULL)
    {
        return false;
    }

    if (address & 0x1U)
    {
        SWD_LOG(
            "AP READ16 ERROR: "
            "Unaligned address 0x%08lX\n",
            (unsigned long)address
        );

        return false;
    }

    /*
     * Align address to 32-bit word boundary and read 32 bits.
     */
    uint32_t aligned_addr = address & ~3UL;
    uint32_t value = 0;

    if (!ap_memory_read32(aligned_addr, &value))
    {
        SWD_LOG(
            "AP READ16 ERROR: "
            "Failed to read 32-bit word at 0x%08lX\n",
            (unsigned long)aligned_addr
        );
        return false;
    }

    /*
     * Extract lower or upper 16-bit half-word based on bit 1.
     */
    if (address & 0x2U) {
        *data = (uint16_t)(value >> 16);
    } else {
        *data = (uint16_t)(value & 0xFFFFU);
    }

    return true;
}

bool ap_set_access_size(uint32_t size)
{
    uint32_t csw;

    if (size != AP_ACCESS_8BIT &&
        size != AP_ACCESS_16BIT &&
        size != AP_ACCESS_32BIT)
    {
        SWD_LOG(
            "AP ERROR: Invalid access size 0x%08lX\n",
            (unsigned long)size
        );

        return false;
    }

    /*
     * Read current CSW.
     *
     * We preserve:
     *
     *   HPROT
     *   DEVICEEN
     *   other configuration bits
     *
     * and modify only SIZE and ADDRINC.
     */
    csw = ap_read(AP_REG_CSW);

    /*
     * Configure transfer size.
     */
    csw &= ~AP_CSW_SIZE_MASK;
    csw |= size;

    /*
     * Configure single address increment.
     */
    csw &= ~AP_CSW_ADDRINC_MASK;
    csw |= AP_CSW_ADDRINC_SINGLE;

    /*
     * Write CSW.
     */
    if (!ap_write(
            AP_REG_CSW,
            csw))
    {
        SWD_LOG(
            "AP ERROR: Failed to write CSW\n"
        );

        return false;
    }

    /*
     * Read back and verify.
     */
    uint32_t verify_csw =
        ap_read(AP_REG_CSW);

    if ((verify_csw & AP_CSW_SIZE_MASK) != size)
    {
        SWD_LOG(
            "AP ERROR: SIZE verification failed\n"
        );

        SWD_LOG(
            "Requested = 0x%08lX\n",
            (unsigned long)size
        );

        SWD_LOG(
            "CSW       = 0x%08lX\n",
            (unsigned long)verify_csw
        );

        return false;
    }

    if ((verify_csw & AP_CSW_ADDRINC_MASK) !=
        AP_CSW_ADDRINC_SINGLE)
    {
        SWD_LOG(
            "AP ERROR: ADDRINC verification failed\n"
        );

        SWD_LOG(
            "CSW = 0x%08lX\n",
            (unsigned long)verify_csw
        );

        return false;
    }

    return true;
}


bool ap_memory_read32(
    uint32_t address,
    uint32_t *data
)
{
    if (data == NULL)
    {
        return false;
    }

    /*
     * 32-bit access requires 4-byte alignment.
     */
    if (address & 0x3U)
    {
        SWD_LOG(
            "AP READ32 ERROR: "
            "Unaligned address 0x%08lX\n",
            (unsigned long)address
        );

        return false;
    }

    /*
     * Configure AHB-AP for 32-bit access.
     */
    if (!ap_set_access_size(
            AP_ACCESS_32BIT))
    {
        return false;
    }

    /*
     * Set target address.
     */
    if (!ap_write(
            AP_REG_TAR,
            address))
    {
        SWD_LOG(
            "AP READ32 ERROR: "
            "Failed to write TAR\n"
        );

        return false;
    }

    /*
     * Start AP read.
     *
     * AP reads are posted.
     */
    uint8_t ack;

    (void)swd_read(
        true,
        AP_REG_DRW,
        &ack
    );

    if (ack != SWD_ACK_OK)
    {
        SWD_LOG(
            "AP READ32 ERROR: "
            "DRW ACK = 0x%02X\n",
            ack
        );

        return false;
    }

    /*
     * Retrieve posted result.
     */
    *data = dp_read_rdbuff();

    return true;
}




bool ap_memory_read(
    uint32_t address,
    uint32_t *data,
    ap_access_size_t size
)
{
    if (data == NULL)
    {
        SWD_LOG(
            "AP READ ERROR: NULL data pointer\n"
        );

        return false;
    }

    switch (size)
    {
        case AP_ACCESS_8BIT:
        {
            /*
             * Read the containing 32-bit word.
             *
             * Example:
             *
             * address = 0x08000003
             *
             * aligned address:
             * 0x08000000
             */
            uint32_t aligned_addr =
                address & ~0x3UL;

            uint32_t value = 0;

            if (!ap_memory_read32(
                    aligned_addr,
                    &value))
            {
                SWD_LOG(
                    "AP READ8 ERROR: "
                    "Failed to read 0x%08lX\n",
                    (unsigned long)aligned_addr
                );

                return false;
            }

            /*
             * Select byte according to
             * address bits [1:0].
             */
            uint32_t shift =
                (address & 0x3U) * 8U;

            *data =
                (value >> shift) & 0xFFU;

            return true;
        }


        case AP_ACCESS_16BIT:
        {
            uint16_t value16 = 0;

            if (!ap_memory_read16(
                    address,
                    &value16))
            {
                return false;
            }

            *data = value16;

            return true;
        }


        case AP_ACCESS_32BIT:
        {
            return ap_memory_read32(
                address,
                data
            );
        }


        default:
        {
            SWD_LOG(
                "AP READ ERROR: "
                "Invalid access size %lu\n",
                (unsigned long)size
            );

            return false;
        }
    }
}




bool ap_memory_write(
    uint32_t address,
    uint32_t data,
    ap_access_size_t size
)
{
    switch (size)
    {
        case AP_ACCESS_8BIT:
        {
            /*
             * 8-bit write.
             *
             * For now, use the AP's 8-bit
             * access-size configuration.
             */

            if (address & 0x0U)
            {
                /* All addresses are valid for byte access. */
            }

            if (!ap_set_access_size(
                    AP_ACCESS_8BIT))
            {
                return false;
            }

            /*
             * Write target address.
             */
            if (!ap_write(
                    AP_REG_TAR,
                    address))
            {
                SWD_LOG(
                    "AP WRITE8 ERROR: "
                    "Failed to write TAR\n"
                );

                return false;
            }

            /*
             * Place byte in the appropriate
             * byte lane.
             */
            uint32_t shift =
                (address & 0x3U) * 8U;

            uint32_t shifted_data =
                (data & 0xFFU) << shift;

            if (!ap_write(
                    AP_REG_DRW,
                    shifted_data))
            {
                SWD_LOG(
                    "AP WRITE8 ERROR: "
                    "Failed to write DRW\n"
                );

                return false;
            }

            return true;
        }


        case AP_ACCESS_16BIT:
        {
            return ap_memory_write16(
                address,
                (uint16_t)data
            );
        }


        case AP_ACCESS_32BIT:
        {
            return ap_memory_write32(
                address,
                data
            );
        }


        default:
        {
            SWD_LOG(
                "AP WRITE ERROR: "
                "Invalid access size %lu\n",
                (unsigned long)size
            );

            return false;
        }
    }
}