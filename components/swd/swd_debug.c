#include "swd_debug.h"

#include "swd.h"
#include "swd_dp.h"
#include "swd_ap.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include "swd_log.h"

static target_state_t
debug_state = TARGET_DISCONNECTED;

static uint32_t
debug_dpidr = 0;

static uint32_t
debug_ap_idr = 0;

static uint32_t
debug_ap_base = 0;


bool debug_connect(void)
{
    SWD_LOG("\n");
    SWD_LOG("=============================\n");
    SWD_LOG(" DEBUG CONNECT\n");
    SWD_LOG("=============================\n");


    /*
     * -----------------------------------------------------
     * 1. Initialize SWD transport
     * -----------------------------------------------------
     */

    swd_init();


    /*
     * -----------------------------------------------------
     * 2. Enter SWD mode
     * -----------------------------------------------------
     */

    swd_jtag_to_swd();


    /*
     * -----------------------------------------------------
     * 3. Read DPIDR
     * -----------------------------------------------------
     */

    debug_dpidr =
        dp_read_idcode();

    if (debug_dpidr == 0)
    {
        SWD_LOG(
            "ERROR: Invalid DPIDR.\n"
        );

        return false;
    }

    SWD_LOG(
        "DPIDR = 0x%08lX\n",
        (unsigned long)debug_dpidr
    );


    /*
     * -----------------------------------------------------
     * 4. Clear sticky errors
     * -----------------------------------------------------
     */

    SWD_LOG(
        "Clearing DP sticky errors...\n"
    );

    if (!dp_clear_errors())
    {
        SWD_LOG(
            "ERROR: Failed to clear DP errors.\n"
        );

        return false;
    }


    /*
     * -----------------------------------------------------
     * 5. Power-up
     * -----------------------------------------------------
     */

    SWD_LOG(
        "Requesting debug power-up...\n"
    );

    if (!dp_power_up())
    {
        SWD_LOG(
            "ERROR: Target power-up failed.\n"
        );

        return false;
    }


    /*
     * -----------------------------------------------------
     * 6. Select AHB-AP
     * -----------------------------------------------------
     */

    if (!dp_select(AP_AHB_APSEL, AP_BANK_F))
    {
        SWD_LOG(
            "ERROR: AP selection failed.\n"
        );

        return false;
    }


    /*
     * -----------------------------------------------------
     * 7. Read AP IDR
     * -----------------------------------------------------
     */

    debug_ap_idr =
        ap_read_idr();

    SWD_LOG(
        "AHB-AP IDR  = 0x%08lX\n",
        (unsigned long)debug_ap_idr
    );


    /*
     * -----------------------------------------------------
     * 8. Read AP BASE
     * -----------------------------------------------------
     */

    debug_ap_base =
        ap_read_base();

    SWD_LOG(
        "AHB-AP BASE = 0x%08lX\n",
        (unsigned long)debug_ap_base
    );


 /*
 * -----------------------------------------------------
 * 9. Switch AHB-AP back to Bank 0
 *
 * CSW, TAR and DRW are in Bank 0.
 * -----------------------------------------------------
 */

if (!dp_select(0, 0))
{
    SWD_LOG(
        "ERROR: AP Bank 0 selection failed.\n"
    );

    return false;
}
if (!ap_configure_32bit())
{
    SWD_LOG(
        "ERROR: AHB-AP configuration failed.\n"
    );

    return false;
}

    /*
     * -----------------------------------------------------
     * 10. Verify CSW
     * -----------------------------------------------------
     */

    if (!ap_verify_csw())
    {
        SWD_LOG(
            "ERROR: AHB-AP CSW verification failed.\n"
        );

        return false;
    }


    debug_state =
        TARGET_CONNECTED;


    SWD_LOG(
        "Target connection successful.\n"
    );

    return true;
}


bool debug_disconnect(void)
{
    debug_state =
        TARGET_DISCONNECTED;

    return true;
}


target_state_t debug_get_state(void)
{
    return debug_state;
}


uint32_t debug_get_dpidr(void)
{
    return debug_dpidr;
}


uint32_t debug_get_ap_idr(void)
{
    return debug_ap_idr;
}


uint32_t debug_get_ap_base(void)
{
    return debug_ap_base;
}


bool debug_reset_target(void)
{
    SWD_LOG("\n");
    SWD_LOG("========================================\n");
    SWD_LOG(" RESET TARGET\n");
    SWD_LOG("========================================\n");

    /*
     * Assert STM32 NRST.
     */
    gpio_set_level(NRST_GPIO, 0);

    /*
     * Keep reset asserted for 10 ms.
     */
    vTaskDelay(pdMS_TO_TICKS(10));

    /*
     * Release NRST.
     */
    gpio_set_level(NRST_GPIO, 1);

    /*
     * Give STM32 time to start executing.
     */
    vTaskDelay(pdMS_TO_TICKS(10));

    SWD_LOG("Target reset released.\n");

    return true;
}