#include "swd_test.h"

#include <stdio.h>
#include "swd_log.h"
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "swd_ap.h"
#include "swd_core.h"
#include "swd_debug.h"
#include "stm32f1_flash.h"


/* =========================================================
 * Test Configuration
 * =========================================================
 */

#define TEST_FLASH_BASE       0x08000000UL

#define TEST_FLASH_PAGE       0x08000000UL

#define TEST_FLASH_SIZE       16U


/* =========================================================
 * Helper
 * =========================================================
 */

static void test_print_header(const char *title)
{
    SWD_LOG("\n");
    SWD_LOG("=============================\n");
    SWD_LOG(" %s\n", title);
    SWD_LOG("=============================\n");
}


/* =========================================================
 * Test: STM32F1 Flash Read
 * =========================================================
 */

bool swd_test_flash_read(void)
{
    uint32_t value;

    test_print_header("FLASH READ TEST");

    /*
     * Read initial vector table entries.
     *
     * 0x08000000:
     * Initial Stack Pointer
     *
     * 0x08000004:
     * Reset Handler
     */

    if (!ap_memory_read32(
            TEST_FLASH_BASE,
            &value))
    {
        SWD_LOG(
            "FLASH[0x%08lX] READ FAILED\n",
            (unsigned long)TEST_FLASH_BASE
        );

        return false;
    }

    SWD_LOG(
        "FLASH[0x%08lX] = 0x%08lX\n",
        (unsigned long)TEST_FLASH_BASE,
        (unsigned long)value
    );


    if (!ap_memory_read32(
            TEST_FLASH_BASE + 4U,
            &value))
    {
        SWD_LOG(
            "FLASH[0x%08lX] READ FAILED\n",
            (unsigned long)(TEST_FLASH_BASE + 4U)
        );

        return false;
    }

    SWD_LOG(
        "FLASH[0x%08lX] = 0x%08lX\n",
        (unsigned long)(TEST_FLASH_BASE + 4U),
        (unsigned long)value
    );

    return true;
}


/* =========================================================
 * Test: Cortex-M3 Debug
 *
 * DEBUG ENABLE
 *      ↓
 *    HALT
 *      ↓
 * REGISTER DUMP
 *      ↓
 *   RESUME
 * =========================================================
 */

bool swd_test_cortex_m3_debug(void)
{
    uint32_t dhcsr;

    test_print_header("CORTEX-M3 DEBUG TEST");


    /*
     * -----------------------------------------------------
     * Read initial DHCSR
     * -----------------------------------------------------
     */

    SWD_LOG(
        "\nReading Cortex-M3 DHCSR...\n"
    );

    dhcsr = core_read_dhcsr();

    SWD_LOG(
        "Initial DHCSR = 0x%08lX\n",
        (unsigned long)dhcsr
    );


    /*
     * -----------------------------------------------------
     * Enable debug
     * -----------------------------------------------------
     */

    SWD_LOG(
        "\nEnabling Cortex-M3 debug...\n"
    );

    if (!core_debug_enable())
    {
        SWD_LOG(
            "DEBUG ENABLE request failed.\n"
        );

        return false;
    }

    SWD_LOG(
        "Debug enable request sent.\n"
    );


    dhcsr = core_read_dhcsr();

    SWD_LOG(
        "DHCSR after DEBUG ENABLE = "
        "0x%08lX\n",
        (unsigned long)dhcsr
    );


    /*
     * -----------------------------------------------------
     * Halt processor
     * -----------------------------------------------------
     */

    SWD_LOG(
        "\nHalting Cortex-M3...\n"
    );

    if (!core_halt())
    {
        SWD_LOG(
            "HALT request failed.\n"
        );

        return false;
    }


    /*
     * Verify halt
     */

    if (!core_is_halted())
    {
        SWD_LOG(
            "CPU did not halt.\n"
        );

        return false;
    }

    SWD_LOG(
        "CPU halted successfully.\n"
    );


    dhcsr = core_read_dhcsr();

    SWD_LOG(
        "DHCSR after HALT = "
        "0x%08lX\n",
        (unsigned long)dhcsr
    );


    if (!(dhcsr & DHCSR_S_HALT))
    {
        SWD_LOG(
            "ERROR: S_HALT is not set.\n"
        );

        return false;
    }


    /*
     * -----------------------------------------------------
     * Register dump
     * -----------------------------------------------------
     */

    SWD_LOG("\n");
    SWD_LOG("=============================\n");
    SWD_LOG(" CORTEX-M3 REGISTER DUMP\n");
    SWD_LOG("=============================\n");

    core_dump_registers();


    /*
     * -----------------------------------------------------
     * Resume processor
     * -----------------------------------------------------
     */

    SWD_LOG(
        "\nResuming Cortex-M3...\n"
    );

    if (!core_resume())
    {
        SWD_LOG(
            "RESUME request failed.\n"
        );

        return false;
    }


    /*
     * Give the target a little time to resume.
     */

    vTaskDelay(
        pdMS_TO_TICKS(10)
    );


    dhcsr = core_read_dhcsr();

    SWD_LOG(
        "DHCSR after RESUME = "
        "0x%08lX\n",
        (unsigned long)dhcsr
    );


    if (dhcsr & DHCSR_S_HALT)
    {
        SWD_LOG(
            "WARNING: CPU still appears halted.\n"
        );

        return false;
    }

    SWD_LOG(
        "CPU resumed successfully.\n"
    );

    return true;
}


/* =========================================================
 * Test: STM32F1 Flash Controller
 * =========================================================
 */

bool swd_test_flash_controller(void)
{
    uint32_t acr;
    uint32_t sr;
    uint32_t cr;

    test_print_header(
        "STM32F1 FLASH CONTROLLER"
    );


    if (!flash_read_register(
            FLASH_ACR,
            &acr))
    {
        SWD_LOG(
            "Failed to read FLASH_ACR\n"
        );

        return false;
    }


    if (!flash_read_register(
            FLASH_SR,
            &sr))
    {
        SWD_LOG(
            "Failed to read FLASH_SR\n"
        );

        return false;
    }


    if (!flash_read_register(
            FLASH_CR,
            &cr))
    {
        SWD_LOG(
            "Failed to read FLASH_CR\n"
        );

        return false;
    }


    SWD_LOG(
        "FLASH_ACR = 0x%08lX\n",
        (unsigned long)acr
    );

    SWD_LOG(
        "FLASH_SR  = 0x%08lX\n",
        (unsigned long)sr
    );

    SWD_LOG(
        "FLASH_CR  = 0x%08lX\n",
        (unsigned long)cr
    );


    SWD_LOG("\n");

    SWD_LOG(
        "BSY      = %lu\n",
        (unsigned long)(
            (sr & FLASH_SR_BSY) != 0
        )
    );

    SWD_LOG(
        "PG       = %lu\n",
        (unsigned long)(
            (cr & FLASH_CR_PG) != 0
        )
    );

    SWD_LOG(
        "PER      = %lu\n",
        (unsigned long)(
            (cr & FLASH_CR_PER) != 0
        )
    );

    SWD_LOG(
        "MER      = %lu\n",
        (unsigned long)(
            (cr & FLASH_CR_MER) != 0
        )
    );

    SWD_LOG(
        "LOCK     = %lu\n",
        (unsigned long)(
            (cr & FLASH_CR_LOCK) != 0
        )
    );

    return true;
}


/* =========================================================
 * Test: Flash Page Erase
 *
 * WARNING:
 * This is a destructive test.
 *
 * Page:
 * 0x08000000
 * =========================================================
 */

bool swd_test_flash_page_erase(void)
{
    test_print_header(
        "FLASH PAGE ERASE TEST"
    );

    SWD_LOG(
        "Erasing Flash page at 0x%08lX...\n",
        (unsigned long)TEST_FLASH_PAGE
    );


    if (!stm32f1_flash_erase_page(
            TEST_FLASH_PAGE))
    {
        SWD_LOG(
            "FLASH PAGE ERASE FAILED!\n"
        );

        return false;
    }


    if (!stm32f1_flash_verify_erased_page(
            TEST_FLASH_PAGE))
    {
        SWD_LOG(
            "FLASH ERASE VERIFY FAILED!\n"
        );

        return false;
    }


    SWD_LOG(
        "FLASH PAGE ERASE VERIFY PASSED!\n"
    );

    return true;
}


/* =========================================================
 * Test: Flash Buffer Programming
 *
 * WARNING:
 * This test programs Flash.
 * =========================================================
 */

bool swd_test_flash_program_buffer(void)
{
    static const uint8_t test_data[] =
    {
        0x11, 0x22,
        0x33, 0x44,
        0x55, 0x66,
        0x77, 0x88,
        0xAA, 0xBB,
        0xCC, 0xDD,
        0xEE, 0xFF,
        0x12, 0x34
    };

    uint32_t value;


    test_print_header(
        "FLASH BUFFER PROGRAM TEST"
    );


    if (!stm32f1_flash_program_buffer(
            TEST_FLASH_BASE,
            test_data,
            sizeof(test_data)))
    {
        SWD_LOG(
            "FLASH BUFFER PROGRAM FAILED!\n"
        );

        return false;
    }


    SWD_LOG(
        "Flash buffer programming completed.\n"
    );


    /*
     * Read back the programmed data
     * as 32-bit words.
     */

    for (uint32_t offset = 0;
         offset < sizeof(test_data);
         offset += 4U)
    {
        uint32_t address =
            TEST_FLASH_BASE + offset;


        if (!ap_memory_read32(
                address,
                &value))
        {
            SWD_LOG(
                "READBACK FAILED at "
                "0x%08lX\n",
                (unsigned long)address
            );

            return false;
        }


        SWD_LOG(
            "FLASH[0x%08lX] = 0x%08lX\n",
            (unsigned long)address,
            (unsigned long)value
        );
    }


    return true;
}


/* =========================================================
 * Test: Flash 16-bit Read
 * =========================================================
 */

bool swd_test_flash_read16(void)
{
    uint16_t value;

    test_print_header(
        "FLASH 16-BIT READ TEST"
    );


    for (uint32_t offset = 0;
         offset < TEST_FLASH_SIZE;
         offset += 2U)
    {
        uint32_t address =
            TEST_FLASH_BASE + offset;


        if (!ap_memory_read16(
                address,
                &value))
        {
            SWD_LOG(
                "READ16 FAILED at "
                "0x%08lX\n",
                (unsigned long)address
            );

            return false;
        }


        SWD_LOG(
            "FLASH[0x%08lX] = 0x%04X\n",
            (unsigned long)address,
            value
        );
    }


    return true;
}


/* =========================================================
 * Test: Generic AP Memory Read
 *
 * Tests:
 *
 *      32-bit
 *      16-bit
 *       8-bit
 * =========================================================
 */

bool swd_test_generic_memory_read(void)
{
    uint32_t data;
    bool result;
    bool all_passed = true;


    test_print_header(
        "GENERIC AP MEMORY READ TEST"
    );


    /*
     * -----------------------------------------------------
     * 32-BIT READ TEST
     * -----------------------------------------------------
     */

    SWD_LOG(
        "\n---- 32-BIT READ TEST ----\n"
    );


    static const struct
    {
        uint32_t address;
        uint32_t expected;

    } read32_tests[] =
    {
        {0x08000000, 0x44332211},
        {0x08000004, 0x88776655},
        {0x08000008, 0xDDCCBBAA},
        {0x0800000C, 0x3412FFEE}
    };


    for (size_t i = 0;
         i < sizeof(read32_tests) /
             sizeof(read32_tests[0]);
         i++)
    {
        data = 0;


        result = ap_memory_read(
            read32_tests[i].address,
            &data,
            AP_ACCESS_32BIT
        );


        if (!result)
        {
            SWD_LOG(
                "READ32 ERROR at 0x%08lX\n",
                (unsigned long)
                read32_tests[i].address
            );

            all_passed = false;

            continue;
        }


        SWD_LOG(
            "READ32 [0x%08lX] = 0x%08lX",
            (unsigned long)
            read32_tests[i].address,

            (unsigned long)data
        );


        if (data ==
            read32_tests[i].expected)
        {
            SWD_LOG("  PASS\n");
        }
        else
        {
            SWD_LOG(
                "  FAIL "
                "(expected 0x%08lX)\n",

                (unsigned long)
                read32_tests[i].expected
            );

            all_passed = false;
        }
    }


    /*
     * -----------------------------------------------------
     * 16-BIT READ TEST
     * -----------------------------------------------------
     */

    SWD_LOG(
        "\n---- 16-BIT READ TEST ----\n"
    );


    static const struct
    {
        uint32_t address;
        uint16_t expected;

    } read16_tests[] =
    {
        {0x08000000, 0x2211},
        {0x08000002, 0x4433},
        {0x08000004, 0x6655},
        {0x08000006, 0x8877},
        {0x08000008, 0xBBAA},
        {0x0800000A, 0xDDCC},
        {0x0800000C, 0xFFEE},
        {0x0800000E, 0x3412}
    };


    for (size_t i = 0;
         i < sizeof(read16_tests) /
             sizeof(read16_tests[0]);
         i++)
    {
        data = 0;


        result = ap_memory_read(
            read16_tests[i].address,
            &data,
            AP_ACCESS_16BIT
        );


        if (!result)
        {
            SWD_LOG(
                "READ16 ERROR at 0x%08lX\n",
                (unsigned long)
                read16_tests[i].address
            );

            all_passed = false;

            continue;
        }


        SWD_LOG(
            "READ16 [0x%08lX] = 0x%04lX",
            (unsigned long)
            read16_tests[i].address,

            (unsigned long)data
        );


        if ((uint16_t)data ==
            read16_tests[i].expected)
        {
            SWD_LOG("  PASS\n");
        }
        else
        {
            SWD_LOG(
                "  FAIL "
                "(expected 0x%04X)\n",

                read16_tests[i].expected
            );

            all_passed = false;
        }
    }


    /*
     * -----------------------------------------------------
     * 8-BIT READ TEST
     * -----------------------------------------------------
     */

    SWD_LOG(
        "\n---- 8-BIT READ TEST ----\n"
    );


    static const struct
    {
        uint32_t address;
        uint8_t expected;

    } read8_tests[] =
    {
        {0x08000000, 0x11},
        {0x08000001, 0x22},
        {0x08000002, 0x33},
        {0x08000003, 0x44},

        {0x08000004, 0x55},
        {0x08000005, 0x66},
        {0x08000006, 0x77},
        {0x08000007, 0x88},

        {0x08000008, 0xAA},
        {0x08000009, 0xBB},
        {0x0800000A, 0xCC},
        {0x0800000B, 0xDD},

        {0x0800000C, 0xEE},
        {0x0800000D, 0xFF},
        {0x0800000E, 0x12},
        {0x0800000F, 0x34}
    };


    for (size_t i = 0;
         i < sizeof(read8_tests) /
             sizeof(read8_tests[0]);
         i++)
    {
        data = 0;


        result = ap_memory_read(
            read8_tests[i].address,
            &data,
            AP_ACCESS_8BIT
        );


        if (!result)
        {
            SWD_LOG(
                "READ8 ERROR at 0x%08lX\n",
                (unsigned long)
                read8_tests[i].address
            );

            all_passed = false;

            continue;
        }


        SWD_LOG(
            "READ8  [0x%08lX] = 0x%02lX",
            (unsigned long)
            read8_tests[i].address,

            (unsigned long)data
        );


        if ((uint8_t)data ==
            read8_tests[i].expected)
        {
            SWD_LOG("  PASS\n");
        }
        else
        {
            SWD_LOG(
                "  FAIL "
                "(expected 0x%02X)\n",

                read8_tests[i].expected
            );

            all_passed = false;
        }
    }


    /*
     * -----------------------------------------------------
     * Final result
     * -----------------------------------------------------
     */

    SWD_LOG("\n");
    SWD_LOG("=============================\n");


    if (all_passed)
    {
        SWD_LOG(
            " ALL GENERIC READ TESTS PASSED!\n"
        );
    }
    else
    {
        SWD_LOG(
            " GENERIC READ TEST FAILED!\n"
        );
    }


    SWD_LOG(
        "=============================\n"
    );


    return all_passed;
}


/* =========================================================
 * Complete SWD Test Suite
 * =========================================================
 *
 * WARNING:
 *
 * The Flash tests are destructive.
 *
 * They erase and program page 0.
 * =========================================================
 */

bool swd_run_all_tests(void)
{
    bool result;


    /*
     * -----------------------------------------------------
     * Test 1: Basic Flash read
     * -----------------------------------------------------
     */

    result = swd_test_flash_read();

    if (!result)
    {
        SWD_LOG(
            "\n[TEST FAILED] Flash read\n"
        );

        return false;
    }


    /*
     * -----------------------------------------------------
     * Test 2: Cortex-M3 debug
     * -----------------------------------------------------
     */

    result = swd_test_cortex_m3_debug();

    if (!result)
    {
        SWD_LOG(
            "\n[TEST FAILED] Cortex-M3 debug\n"
        );

        return false;
    }


    /*
     * -----------------------------------------------------
     * Test 3: Flash controller
     * -----------------------------------------------------
     */

    result = swd_test_flash_controller();

    if (!result)
    {
        SWD_LOG(
            "\n[TEST FAILED] Flash controller\n"
        );

        return false;
    }


    /*
     * -----------------------------------------------------
     * Unlock Flash
     * -----------------------------------------------------
     */

    SWD_LOG("\n");
    SWD_LOG(
        "Unlocking STM32F1 Flash...\n"
    );


    if (!stm32f1_flash_unlock())
    {
        SWD_LOG(
            "Flash unlock failed.\n"
        );

        return false;
    }


    /*
     * -----------------------------------------------------
     * Test 4: Page erase
     * -----------------------------------------------------
     */

    result = swd_test_flash_page_erase();

    if (!result)
    {
        SWD_LOG(
            "\n[TEST FAILED] Flash page erase\n"
        );

        return false;
    }


    /*
     * -----------------------------------------------------
     * Test 5: Flash programming
     * -----------------------------------------------------
     */

    result = swd_test_flash_program_buffer();

    if (!result)
    {
        SWD_LOG(
            "\n[TEST FAILED] Flash programming\n"
        );

        return false;
    }


    /*
     * -----------------------------------------------------
     * Test 6: 16-bit Flash read
     * -----------------------------------------------------
     */

    result = swd_test_flash_read16();

    if (!result)
    {
        SWD_LOG(
            "\n[TEST FAILED] Flash 16-bit read\n"
        );

        return false;
    }


    /*
     * -----------------------------------------------------
     * Test 7: Generic memory API
     * -----------------------------------------------------
     */

    result = swd_test_generic_memory_read();

    if (!result)
    {
        SWD_LOG(
            "\n[TEST FAILED] Generic memory read\n"
        );

        return false;
    }


    /*
     * -----------------------------------------------------
     * All tests passed
     * -----------------------------------------------------
     */

    SWD_LOG("\n");
    SWD_LOG("=============================\n");
    SWD_LOG(" ALL SWD TESTS PASSED!\n");
    SWD_LOG("=============================\n");


    return true;
}



bool swd_test_programmed_firmware_execution(void)
{
    uint32_t pc = 0;
    uint32_t sp = 0;
    uint32_t xpsr = 0;
    uint32_t dhcsr = 0;

    SWD_LOG("\n========================================\n");
    SWD_LOG(" VERIFY FIRMWARE EXECUTION\n");
    SWD_LOG("========================================\n");

    /*
     * Reset the STM32 target.
     */
    if (!debug_reset_target()) {
        SWD_LOG("Target reset FAILED.\n");
        return false;
    }

    /*
     * Give the target some time to execute.
     */
    vTaskDelay(pdMS_TO_TICKS(20));

    /*
     * Halt the CPU so we can inspect its state.
     */
    if (!core_halt()) {
        SWD_LOG("CPU halt FAILED.\n");
        return false;
    }

    /*
     * Read CPU registers.
     */
    if (!core_read_register(CORE_REG_PC, &pc)) {
        SWD_LOG("Failed to read PC\n");
        return false;
    }

    if (!core_read_register(CORE_REG_SP, &sp)) {
        SWD_LOG("Failed to read SP\n");
        return false;
    }

    if (!core_read_register(CORE_REG_XPSR, &xpsr)) {
        SWD_LOG("Failed to read XPSR\n");
        return false;
    }

    dhcsr = core_read_dhcsr();
    (void)dhcsr;
    (void)sp;
    (void)pc;
    (void)xpsr;

    SWD_LOG("\nCPU STATE\n");
    SWD_LOG("----------------------------------------\n");
    SWD_LOG("SP    = 0x%08lX\n", (unsigned long)sp);
    SWD_LOG("PC    = 0x%08lX\n", (unsigned long)pc);
    SWD_LOG("xPSR  = 0x%08lX\n", (unsigned long)xpsr);
    SWD_LOG("DHCSR = 0x%08lX\n", (unsigned long)dhcsr);


    SWD_LOG("\nResuming CPU to allow firmware to run...\n");
    if (!core_resume()) {
        SWD_LOG("CPU resume FAILED.\n");
        return false;
    }
    /* ----------------------- */



    return true;
}