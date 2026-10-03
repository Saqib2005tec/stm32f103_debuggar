#include "swd_dp.h"
#include "swd.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include "swd_log.h"

uint32_t dp_read(uint8_t addr)
{
    uint8_t ack;

    uint32_t data =
        swd_read(
            false, // APnDP = 0 → DP
            addr,
            &ack);

    return data;
}

bool dp_write(uint8_t addr,uint32_t data)
{
    return swd_write(
        false, // APnDP = 0 → DP
        addr,
        data);
}

uint32_t dp_read_idcode(void)
{
    return dp_read(DP_REG_DPIDR);
}

uint32_t dp_read_ctrl_stat(void)
{
    return dp_read(DP_REG_CTRL_STAT);
}

bool dp_power_up(void)
{
    const uint32_t power_up =
        DP_CTRL_CDBGPWRUPREQ |
        DP_CTRL_CSYSPWRUPREQ;

    if (!dp_write(
            DP_REG_CTRL_STAT,
            power_up))
    {
        SWD_LOG(
            "Failed to request DP power-up.\n"
        );

        return false;
    }


    /*
     * Poll CTRL/STAT.
     */

    for (int i = 0; i < 100; i++)
    {
        uint32_t ctrl_stat =
            dp_read_ctrl_stat();

        uint32_t ack =
            ctrl_stat &
            DP_POWER_ACK_MASK;

        if (ack == DP_POWER_ACK_MASK)
        {
            SWD_LOG(
                "Debug and system power-up ACK received.\n"
            );

            return true;
        }

        vTaskDelay(
            pdMS_TO_TICKS(1)
        );
    }


    SWD_LOG(
        "ERROR: Power-up ACK timeout.\n"
    );

    return false;
}

bool dp_select(
    uint8_t apsel,
    uint8_t apbanksel)
{
    uint32_t value = 0;

    /*
     * APSEL is bits [31:24]
     */
    value |=
        ((uint32_t)apsel << 24);

    /*
     * APBANKSEL is bits [7:4]
     */
    value |=
        ((uint32_t)(apbanksel & 0x0F) << 4);

    return dp_write(
        DP_REG_SELECT,
        value);
}

uint32_t dp_read_rdbuff(void)
{
    return dp_read(DP_REG_RDBUFF);
}

bool dp_clear_errors(void)
{
    uint32_t abort_value =
        DP_ABORT_STKCMPCLR |
        DP_ABORT_STKERRCLR |
        DP_ABORT_WDERRCLR |
        DP_ABORT_ORUNERRCLR;

    return dp_write(
        DP_REG_ABORT,
        abort_value
    );
}

void dp_print_ctrl_stat(
    uint32_t value
)
{
    SWD_LOG("\n");
    SWD_LOG("=============================\n");
    SWD_LOG(" DP CTRL/STAT\n");
    SWD_LOG("=============================\n");

    SWD_LOG(
        "CTRL/STAT = 0x%08lX\n",
        (unsigned long)value
    );

    SWD_LOG(
        "CDBGPWRUPREQ  : %s\n",
        (value & DP_CTRL_CDBGPWRUPREQ)
            ? "1"
            : "0"
    );

    SWD_LOG(
        "CDBGPWRUPACK  : %s\n",
        (value & DP_CTRL_CDBGPWRUPACK)
            ? "1"
            : "0"
    );

    SWD_LOG(
        "CSYSPWRUPREQ  : %s\n",
        (value & DP_CTRL_CSYSPWRUPREQ)
            ? "1"
            : "0"
    );

    SWD_LOG(
        "CSYSPWRUPACK  : %s\n",
        (value & DP_CTRL_CSYSPWRUPACK)
            ? "1"
            : "0"
    );

    SWD_LOG(
        "STICKYERR     : %s\n",
        (value & DP_CTRL_STICKYERR)
            ? "1"
            : "0"
    );

    SWD_LOG(
        "STICKYCMP     : %s\n",
        (value & DP_CTRL_STICKYCMP)
            ? "1"
            : "0"
    );

    SWD_LOG(
        "STICKYORUN    : %s\n",
        (value & DP_CTRL_STICKYORUN)
            ? "1"
            : "0"
    );

    SWD_LOG("=============================\n");
}