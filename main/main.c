#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "debugger_ui.h"
#include "swd_debug.h"
#include "swd_test.h"
#include "stm32f1_flash.h"
#include "swd_core.h"

extern const uint8_t _binary_stm32f103_blink_bin_start[];
extern const uint8_t _binary_stm32f103_blink_bin_end[];

static bool fail_and_stop(const char *reason)
{
    ui_footer_failure(reason);
    return false;
}

static void on_erase_verify_progress(uint32_t current_addr, uint32_t current_bytes, uint32_t total_bytes, void *user_data)
{
    (void)user_data;
    ui_verify_erase_progress(current_addr, current_bytes, total_bytes);
}

static bool flash_target_firmware(const uint8_t *firmware, uint32_t firmware_size)
{
    const uint64_t start_ms = ui_time_ms();

    /* ---------------------------------------------------------
     * Halt CPU
     * --------------------------------------------------------- */
    ui_section("TARGET CONTROL");
    ui_step_start("Halting Cortex-M3");

    if (!core_halt())
    {
        ui_step_fail("halt failed");
        return fail_and_stop("Could not halt target CPU");
    }

    ui_step_ok();

    /* ---------------------------------------------------------
     * Validate image
     * --------------------------------------------------------- */
    ui_section("FIRMWARE VALIDATION");

    if (!stm32f1_validate_bin(firmware, firmware_size))
    {
        ui_step_start("Validating BIN image");
        ui_step_fail("invalid image");
        return fail_and_stop("BIN image validation failed");
    }

    /* Vector table information is already validated by the function. */
    uint32_t initial_msp =
        ((uint32_t)firmware[0]) |
        ((uint32_t)firmware[1] << 8) |
        ((uint32_t)firmware[2] << 16) |
        ((uint32_t)firmware[3] << 24);

    uint32_t reset_handler =
        ((uint32_t)firmware[4]) |
        ((uint32_t)firmware[5] << 8) |
        ((uint32_t)firmware[6] << 16) |
        ((uint32_t)firmware[7] << 24);

    ui_firmware_info(
        STM32_FLASH_START,
        firmware_size,
        initial_msp,
        reset_handler
    );

    ui_step_start("BIN image validation");
    ui_step_ok();

    /* ---------------------------------------------------------
     * Mass erase
     * --------------------------------------------------------- */
    ui_section("FLASH ERASE");
    ui_spinner_start("Mass erasing STM32F1 Flash");

    if (!stm32f1_flash_mass_erase())
    {
        ui_spinner_stop(false);
        return fail_and_stop("Flash mass erase failed");
    }

    ui_spinner_stop(true);

    /* ---------------------------------------------------------
     * Verify erase
     * --------------------------------------------------------- */
    vTaskDelay(pdMS_TO_TICKS(20));
    ui_verify_erase_progress(STM32_FLASH_START, 0, STM32_FLASH_SIZE);

    if (!stm32f1_flash_verify_erased_with_progress(on_erase_verify_progress, NULL))
    {
        ui_verify_erase_done(false);
        return fail_and_stop("Flash erase verification failed");
    }

    ui_verify_erase_done(true);

    /* ---------------------------------------------------------
     * Program firmware
     * --------------------------------------------------------- */
    ui_section("FLASH PROGRAMMING");
    printf("  ├─ Address    0x%08lX\n", (unsigned long)STM32_FLASH_START);
    printf("  ├─ Image      %lu bytes\n", (unsigned long)firmware_size);
    printf("  └─ Writing    ");
    fflush(stdout);

    ui_spinner_start("Programming firmware");

    if (!stm32f1_flash_program_buffer(
            STM32_FLASH_START,
            firmware,
            firmware_size))
    {
        ui_spinner_stop(false);
        return fail_and_stop("Flash programming failed");
    }

    ui_spinner_stop(true);

    /* ---------------------------------------------------------
     * Verify firmware
     * --------------------------------------------------------- */
    ui_section("FLASH VERIFICATION");
    ui_spinner_start("Comparing programmed image");

    if (!stm32f1_flash_verify_buffer(
            STM32_FLASH_START,
            firmware,
            firmware_size))
    {
        ui_spinner_stop(false);
        return fail_and_stop("Flash verification failed");
    }

    ui_spinner_stop(true);

    /* ---------------------------------------------------------
     * Reset + execute
     * --------------------------------------------------------- */
    ui_section("TARGET STARTUP");
    ui_step_start("Resetting target");

    if (!debug_reset_target())
    {
        ui_step_fail("reset failed");
        return fail_and_stop("Target reset failed");
    }

    ui_step_ok();

    ui_step_start("Starting programmed firmware");

    if (!swd_test_programmed_firmware_execution())
    {
        ui_step_fail("execution check failed");
        return fail_and_stop("Firmware execution verification failed");
    }

    ui_step_ok();

    const uint64_t elapsed_ms = ui_time_ms() - start_ms;
    ui_footer_success(firmware_size, elapsed_ms);
    return true;
}

static void dump_target_registers(void)
{
    if (!core_is_halted())
    {
        printf("\n" UI_RED "  ✗ Target CPU is running. Halt first ('h') to inspect registers." UI_RESET "\n");
        return;
    }

    uint32_t regs[17] = {0};
    bool valid[17] = {false};

    for (uint8_t i = 0; i <= CORE_REG_XPSR; i++)
    {
        valid[i] = core_read_register(i, &regs[i]);
    }

    ui_registers_view(regs, valid);
}

static bool read_line(char *buf, size_t max_len)
{
    size_t idx = 0;
    while (idx < max_len - 1)
    {
        int c = fgetc(stdin);
        if (c == EOF || c < 0)
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (c == '\r' || c == '\n')
        {
            if (idx > 0)
            {
                putchar('\n');
                break;
            }
            continue;
        }

        if (c == 0x08 || c == 0x7F) /* Backspace / Delete */
        {
            if (idx > 0)
            {
                idx--;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }

        if (c >= 32 && c <= 126)
        {
            buf[idx++] = (char)c;
            putchar((char)c);
            fflush(stdout);
        }
    }
    buf[idx] = '\0';
    return idx > 0;
}

static void show_breakpoints_table(void)
{
    ui_breakpoint_info_t bps[FP_MAX_BREAKPOINTS];
    for (uint8_t i = 0; i < FP_MAX_BREAKPOINTS; i++)
    {
        bps[i].index = i;
        bps[i].enabled = false;
        bps[i].address = 0;
        core_bp_get(i, &bps[i].enabled, &bps[i].address);
    }
    ui_breakpoints_view(bps, FP_MAX_BREAKPOINTS);
}

static void handle_breakpoint_menu(void)
{
    show_breakpoints_table();

    while (1)
    {
        printf("  [s] Set address   [c] Clear slot   [x] Clear all   [l] List   [q] Return\n");
        printf(UI_BOLD UI_CYAN "  [bp]> " UI_RESET);
        fflush(stdout);

        char cmd[16];
        if (!read_line(cmd, sizeof(cmd)))
        {
            continue;
        }

        char op = cmd[0];
        if (op == 'q' || op == 'Q')
        {
            printf("  Returning to main menu.\n");
            break;
        }
        else if (op == 'l' || op == 'L')
        {
            show_breakpoints_table();
        }
        else if (op == 's' || op == 'S')
        {
            int8_t free_slot = core_bp_find_free_slot();
            if (free_slot < 0)
            {
                printf(UI_RED "  ✗ All %u hardware breakpoint slots are full. Clear one first." UI_RESET "\n",
                       FP_MAX_BREAKPOINTS);
                continue;
            }

            printf("  Enter target address in hex (e.g. 0x0800013C): ");
            fflush(stdout);

            char addr_str[32];
            if (read_line(addr_str, sizeof(addr_str)))
            {
                char *end = NULL;
                uint32_t addr = strtoul(addr_str, &end, 16);
                if (end == addr_str || addr == 0)
                {
                    printf(UI_RED "  ✗ Invalid hex address '%s'." UI_RESET "\n", addr_str);
                }
                else
                {
                    /* Hardware instruction breakpoints are halfword aligned */
                    addr &= ~1UL;
                    if (core_bp_set((uint8_t)free_slot, addr))
                    {
                        printf(UI_GREEN "  ✓ Breakpoint %d set at 0x%08lX" UI_RESET "\n",
                               free_slot, (unsigned long)addr);
                    }
                    else
                    {
                        printf(UI_RED "  ✗ Failed to set breakpoint for slot %d" UI_RESET "\n",
                               free_slot);
                    }
                    show_breakpoints_table();
                }
            }
        }
        else if (op == 'c' || op == 'C')
        {
            printf("  Enter slot number to clear (0-%u): ", FP_MAX_BREAKPOINTS - 1);
            fflush(stdout);

            char slot_str[16];
            if (read_line(slot_str, sizeof(slot_str)))
            {
                int slot = slot_str[0] - '0';
                if (slot >= 0 && slot < FP_MAX_BREAKPOINTS)
                {
                    if (core_bp_clear((uint8_t)slot))
                    {
                        printf(UI_GREEN "  ✓ Breakpoint slot %d cleared." UI_RESET "\n", slot);
                    }
                    else
                    {
                        printf(UI_RED "  ✗ Failed to clear breakpoint slot %d." UI_RESET "\n", slot);
                    }
                    show_breakpoints_table();
                }
                else
                {
                    printf(UI_RED "  ✗ Invalid slot number '%s'." UI_RESET "\n", slot_str);
                }
            }
        }
        else if (op == 'x' || op == 'X')
        {
            if (core_bp_clear_all())
            {
                printf(UI_GREEN "  ✓ All %u hardware breakpoints cleared." UI_RESET "\n",
                       FP_MAX_BREAKPOINTS);
            }
            else
            {
                printf(UI_RED "  ✗ Failed to clear all breakpoints." UI_RESET "\n");
            }
            show_breakpoints_table();
        }
        else
        {
            printf(UI_YELLOW "  Unknown option '%c'. Type 's', 'c', 'x', 'l', or 'q'." UI_RESET "\n", op);
        }
    }
}

static void debugger_interactive_menu(const uint8_t *firmware, uint32_t firmware_size)
{
    ui_menu_show();
    ui_menu_prompt();

    bool target_was_running = !core_is_halted();
    uint32_t poll_ticks = 0;

    while (1)
    {
        int c = fgetc(stdin);
        if (c == EOF || c < 0)
        {
            vTaskDelay(pdMS_TO_TICKS(25));

            /* Check if running target has hit a hardware breakpoint */
            if (target_was_running)
            {
                poll_ticks++;
                if (poll_ticks >= 3) /* Every ~75ms */
                {
                    poll_ticks = 0;
                    if (core_is_halted())
                    {
                        target_was_running = false;
                        uint32_t hit_pc = 0;
                        core_read_register(CORE_REG_PC, &hit_pc);

                        if (core_is_breakpoint_hit() || core_bp_is_active_at(hit_pc))
                        {
                            printf("\n" UI_YELLOW UI_BOLD "  ⚡ BREAKPOINT HIT! Cortex-M3 halted at PC: 0x%08lX" UI_RESET "\n",
                                   (unsigned long)hit_pc);
                        }
                        else
                        {
                            printf("\n" UI_YELLOW "  ● Target CPU halted at PC: 0x%08lX" UI_RESET "\n",
                                   (unsigned long)hit_pc);
                        }
                        ui_menu_prompt();
                    }
                }
            }
            continue;
        }

        /* Ignore whitespace and carriage returns / newlines */
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t')
        {
            continue;
        }

        /* Filter ANSI escape sequences (e.g. arrow keys ESC [ A) */
        if (c == 27)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            while ((c = fgetc(stdin)) != EOF && c >= 0)
            {
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '~')
                {
                    break;
                }
            }
            continue;
        }

        /* Echo the key entered */
        printf("%c\n", (char)c);

        switch (c)
        {
        case 'h':
        case 'H':
            ui_step_start("Halting Cortex-M3");
            if (core_halt())
            {
                target_was_running = false;
                uint32_t pc = 0;
                if (core_read_register(CORE_REG_PC, &pc))
                {
                    printf(UI_GREEN "✓ Halted at PC: 0x%08lX" UI_RESET "\n", (unsigned long)pc);
                }
                else
                {
                    ui_step_ok();
                }
            }
            else
            {
                ui_step_fail("halt failed");
            }
            break;

        case 'r':
        case 'R':
            ui_step_start("Resuming Cortex-M3");
            if (core_resume_with_stepover())
            {
                target_was_running = true;
                printf(UI_GREEN "✓ Running" UI_RESET "\n");
            }
            else
            {
                ui_step_fail("resume failed");
            }
            break;

        case 's':
        case 'S':
            if (!core_is_halted())
            {
                printf("\n" UI_YELLOW "  ⚠ Target CPU is running. Halt first ('h') before single stepping." UI_RESET "\n");
            }
            else
            {
                ui_step_start("Stepping single instruction");
                if (core_step_with_stepover())
                {
                    target_was_running = false;
                    uint32_t pc = 0;
                    if (core_read_register(CORE_REG_PC, &pc))
                    {
                        printf(UI_GREEN "✓ Stepped to PC: 0x%08lX" UI_RESET "\n", (unsigned long)pc);
                    }
                    else
                    {
                        ui_step_ok();
                    }
                }
                else
                {
                    ui_step_fail("step failed");
                }
            }
            break;

        case 'b':
        case 'B':
            handle_breakpoint_menu();
            ui_menu_show();
            break;

        case 'c':
        case 'C':
            {
                bool halted = core_is_halted();
                uint32_t dhcsr = core_read_dhcsr();
                printf("\n  " UI_BOLD "TARGET STATUS" UI_RESET "\n");
                printf("  ├─ State:     %s\n", halted ? UI_YELLOW UI_BOLD "HALTED" UI_RESET : UI_GREEN UI_BOLD "RUNNING" UI_RESET);
                printf("  ├─ DHCSR:     0x%08lX\n", (unsigned long)dhcsr);
                if (halted)
                {
                    uint32_t pc = 0;
                    if (core_read_register(CORE_REG_PC, &pc))
                    {
                        printf("  └─ PC:        0x%08lX\n", (unsigned long)pc);
                    }
                    else
                    {
                        printf("  └─ PC:        [read error]\n");
                    }
                }
                else
                {
                    printf("  └─ Info:      Target CPU is actively running code\n");
                }
                printf("\n");
            }
            break;

        case 'd':
        case 'D':
            dump_target_registers();
            break;

        case 't':
        case 'T':
            ui_step_start("Resetting target MCU");
            if (debug_reset_target())
            {
                core_debug_enable();
                target_was_running = true;
                printf(UI_GREEN "✓ Reset complete (Running)" UI_RESET "\n");
            }
            else
            {
                ui_step_fail("reset failed");
            }
            break;

        case 'f':
        case 'F':
            printf("\n" UI_YELLOW "  ● Triggering firmware re-flash..." UI_RESET "\n");
            flash_target_firmware(firmware, firmware_size);
            target_was_running = true;
            ui_menu_show();
            break;

        case 'm':
        case 'M':
        case '?':
            ui_menu_show();
            break;

        default:
            printf(UI_RED "  ✗ Unknown command '%c'. Type 'm' to show menu." UI_RESET "\n", (char)c);
            break;
        }

        ui_menu_prompt();
    }
}

void app_main(void)
{
    const uint8_t *firmware = _binary_stm32f103_blink_bin_start;
    const uint32_t firmware_size = (uint32_t)(
        _binary_stm32f103_blink_bin_end -
        _binary_stm32f103_blink_bin_start
    );

    ui_init();
    ui_header();

    /* ---------------------------------------------------------
     * Connect to target
     * --------------------------------------------------------- */
    ui_section("CONNECTING TO TARGET");
    ui_spinner_start("Establishing SWD connection");

    if (!debug_connect())
    {
        ui_spinner_stop(false);
        fail_and_stop("SWD connection failed");
        return;
    }

    ui_spinner_stop(true);
    ui_target_info("STM32F103C6T6", "ARM Cortex-M3");

    /* Program initial firmware */
    if (!flash_target_firmware(firmware, firmware_size))
    {
        printf("\n" UI_YELLOW "  Flash failed. Entering control menu to allow debug / retry." UI_RESET "\n");
    }

    /* Start interactive control dropdown menu */
    debugger_interactive_menu(firmware, firmware_size);
}
