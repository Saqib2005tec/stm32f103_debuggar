#include "debugger_ui.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_timer.h"


#define SPINNER_TASK_STACK 3072
#define SPINNER_TASK_PRIO  4

static TaskHandle_t spinner_task_handle = NULL;
static volatile bool spinner_running = false;
static char spinner_label[64];

static void spinner_task(void *arg)
{
    (void)arg;

    static const char frames[] = "|/-\\";
    unsigned int i = 0;

    while (spinner_running)
    {
        printf("\r  %s%s%s %c", UI_CYAN, spinner_label,
               UI_RESET, frames[i++ & 3U]);
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(90));
    }

    spinner_task_handle = NULL;
    vTaskDelete(NULL);
}

uint64_t ui_time_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000ULL);
}

void ui_init(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);
}

void ui_header(void)
{
    printf("\033[2J\033[H");
    printf("\n");
    printf(UI_CYAN UI_BOLD);
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                 ESP32 SWD DEBUGGER                           ║\n");
    printf("║              STM32F103 • Cortex-M3                           ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf(UI_RESET);
    printf("\n");
}

void ui_section(const char *title)
{
    printf("\n" UI_BOLD UI_CYAN "  ● %s" UI_RESET "\n", title);
}

void ui_step_start(const char *label)
{
    printf("  ├─ %-42s ", label);
    fflush(stdout);
}

void ui_step_ok(void)
{
    printf(UI_GREEN "✓" UI_RESET "\n");
}

void ui_step_fail(const char *reason)
{
    printf(UI_RED "✗" UI_RESET);
    if (reason && reason[0])
        printf("  %s", reason);
    printf("\n");
}

void ui_spinner_start(const char *label)
{
    if (spinner_running)
        return;

    snprintf(spinner_label, sizeof(spinner_label), "%s", label);
    spinner_running = true;

    xTaskCreate(
        spinner_task,
        "ui_spinner",
        SPINNER_TASK_STACK,
        NULL,
        SPINNER_TASK_PRIO,
        &spinner_task_handle
    );
}

void ui_spinner_stop(bool success)
{
    if (!spinner_running)
        return;

    spinner_running = false;

    /* Give the spinner task a moment to observe the flag. */
    for (int i = 0; i < 20 && spinner_task_handle != NULL; ++i)
        vTaskDelay(pdMS_TO_TICKS(10));

    printf("\r  %-58s\r", "");
    printf("  %s %s\n", success ? UI_GREEN "✓" UI_RESET
                              : UI_RED "✗" UI_RESET,
            spinner_label);
}

void ui_progress(uint32_t current, uint32_t total)
{
    const unsigned width = 36;
    unsigned filled = 0;

    if (total > 0)
        filled = (unsigned)(((uint64_t)current * width) / total);

    if (filled > width)
        filled = width;

    printf("\r    [");
    for (unsigned i = 0; i < width; ++i)
        putchar(i < filled ? '#' : '-');

    unsigned percent = total ? (unsigned)(((uint64_t)current * 100ULL) / total) : 0;
    if (percent > 100)
        percent = 100;

    printf("] %3u%%  %lu/%lu bytes",
           percent,
           (unsigned long)current,
           (unsigned long)total);
    fflush(stdout);
}

void ui_progress_done(bool success)
{
    printf("  %s\n", success ? UI_GREEN "✓ complete" UI_RESET
                             : UI_RED "✗ failed" UI_RESET);
}

void ui_verify_erase_progress(uint32_t address, uint32_t current, uint32_t total)
{
    static const char *braille_frames[] = {
        "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
    };
    static unsigned frame_idx = 0;
    static uint64_t last_update_ms = 0;

    if (current == 0)
    {
        frame_idx = 0;
        last_update_ms = 0;
    }

    uint64_t now = ui_time_ms();
    /* Throttle to ~40-50 fps (every 20ms) during active verification, but allow start and end */
    if (current > 0 && current < total && (now - last_update_ms < 20))
    {
        return;
    }
    last_update_ms = now;

    const char *frame = braille_frames[frame_idx % 10];
    frame_idx++;

    uint32_t current_kb = current / 1024U;
    uint32_t total_kb = total / 1024U;

    printf("\r  %s%s%s Verifying erased Flash... @ 0x%08lX (%2lu KB / %2lu KB)   ",
           UI_CYAN, frame, UI_RESET,
           (unsigned long)address,
           (unsigned long)current_kb,
           (unsigned long)total_kb);
    fflush(stdout);
}

void ui_verify_erase_done(bool success)
{
    if (success)
    {
        printf("\r  %-64s\r", "");
        printf("  %s Verifying erased Flash\n", UI_GREEN "✓" UI_RESET);
    }
    else
    {
        printf("\n  %s Verifying erased Flash\n", UI_RED "✗" UI_RESET);
    }
    fflush(stdout);
}

void ui_target_info(const char *target, const char *core)
{
    printf("\n  " UI_BOLD "TARGET" UI_RESET "\n");
    printf("  ┌────────────────────────────────────────────────────────┐\n");
    printf("  │ MCU        %-42s │\n", target);
    printf("  │ Core       %-42s │\n", core);
    printf("  │ Interface  %-42s │\n", "SWD");
    printf("  └────────────────────────────────────────────────────────┘\n");
}

void ui_firmware_info(uint32_t address, uint32_t size,
                      uint32_t msp, uint32_t reset_handler)
{
    printf("\n  " UI_BOLD "FIRMWARE IMAGE" UI_RESET "\n");
    printf("  ├─ Flash      0x%08lX\n", (unsigned long)address);
    printf("  ├─ Size       %lu bytes\n", (unsigned long)size);
    printf("  ├─ Initial MSP 0x%08lX\n", (unsigned long)msp);
    printf("  └─ Reset      0x%08lX\n", (unsigned long)reset_handler);
}

void ui_footer_success(uint32_t image_size, uint64_t elapsed_ms)
{
    printf("\n");
    printf(UI_GREEN UI_BOLD);
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                    PROGRAMMING COMPLETE                    ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf(UI_RESET);
    printf("║  Image       %6lu bytes                                  ║\n",
           (unsigned long)image_size);
    printf("║  Flash       0x08000000                                  ║\n");
    printf("║  Verify      PASSED                                      ║\n");
    printf("║  CPU         RUNNING                                     ║\n");
    printf("║  Time        %6llu ms                                   ║\n",
           (unsigned long long)elapsed_ms);
    printf(UI_GREEN UI_BOLD);
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf(UI_RESET);
}

void ui_footer_failure(const char *reason)
{
    printf("\n");
    printf(UI_RED UI_BOLD);
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                    OPERATION FAILED                       ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf(UI_RESET);
    if (reason)
        printf("  %s\n", reason);
}

void ui_menu_show(void)
{
    printf("\n");
    printf(UI_CYAN UI_BOLD);
    printf("  ▼ TARGET CONTROL MENU\n");
    printf("  ┌─────┬──────────────────────────────────────────────────┐\n");
    printf("  │ KEY │ %-48s │\n", "ACTION");
    printf("  ├─────┼──────────────────────────────────────────────────┤\n");
    printf(UI_RESET);
    printf("  │  " UI_BOLD UI_YELLOW "h" UI_RESET "  │ %-48s │\n", "Halt target CPU");
    printf("  │  " UI_BOLD UI_YELLOW "r" UI_RESET "  │ %-48s │\n", "Resume target CPU");
    printf("  │  " UI_BOLD UI_YELLOW "s" UI_RESET "  │ %-48s │\n", "Single step instruction");
    printf("  │  " UI_BOLD UI_YELLOW "c" UI_RESET "  │ %-48s │\n", "Check CPU status (Run / Halt & PC)");
    printf("  │  " UI_BOLD UI_YELLOW "d" UI_RESET "  │ %-48s │\n", "Dump CPU registers (R0-R12, SP, LR, PC, xPSR)");
    printf("  │  " UI_BOLD UI_YELLOW "b" UI_RESET "  │ %-48s │\n", "Hardware breakpoints (Set, Clear, List)");
    printf("  │  " UI_BOLD UI_YELLOW "t" UI_RESET "  │ %-48s │\n", "Reset target MCU");
    printf("  │  " UI_BOLD UI_YELLOW "f" UI_RESET "  │ %-48s │\n", "Re-flash firmware image");
    printf("  │  " UI_BOLD UI_YELLOW "m" UI_RESET "  │ %-48s │\n", "Show / drop down this menu");
    printf(UI_CYAN UI_BOLD);
    printf("  └─────┴──────────────────────────────────────────────────┘\n");
    printf(UI_RESET "\n");
}

void ui_menu_prompt(void)
{
    printf(UI_BOLD UI_CYAN "  [debugger]> " UI_RESET);
    fflush(stdout);
}

void ui_registers_view(const uint32_t regs[17], const bool valid[17])
{
    printf("\n  " UI_BOLD "REGISTER DUMP" UI_RESET "\n");
    printf(UI_CYAN);
    printf("  ┌────────────────────────────────────────────────────────┐\n");
    printf(UI_RESET);

    for (int i = 0; i <= 6; i++)
    {
        int r_left = i;
        int r_right = i + 7;
        char left_buf[24];
        char right_buf[24];

        if (valid[r_left])
            snprintf(left_buf, sizeof(left_buf), "R%-2d  0x%08lX", r_left, (unsigned long)regs[r_left]);
        else
            snprintf(left_buf, sizeof(left_buf), "R%-2d  ERROR", r_left);

        if (valid[r_right])
            snprintf(right_buf, sizeof(right_buf), "R%-2d 0x%08lX", r_right, (unsigned long)regs[r_right]);
        else
            snprintf(right_buf, sizeof(right_buf), "R%-2d ERROR", r_right);

        printf("  │  %-24s   %-25s │\n", left_buf, right_buf);
    }

    char sp_buf[24];
    char lr_buf[24];
    char pc_buf[24];
    char xpsr_buf[24];

    if (valid[13]) snprintf(sp_buf, sizeof(sp_buf), "SP   0x%08lX", (unsigned long)regs[13]);
    else snprintf(sp_buf, sizeof(sp_buf), "SP   ERROR");

    if (valid[14]) snprintf(lr_buf, sizeof(lr_buf), "LR   0x%08lX", (unsigned long)regs[14]);
    else snprintf(lr_buf, sizeof(lr_buf), "LR   ERROR");

    if (valid[15]) snprintf(pc_buf, sizeof(pc_buf), "PC   0x%08lX", (unsigned long)regs[15]);
    else snprintf(pc_buf, sizeof(pc_buf), "PC   ERROR");

    if (valid[16]) snprintf(xpsr_buf, sizeof(xpsr_buf), "xPSR 0x%08lX", (unsigned long)regs[16]);
    else snprintf(xpsr_buf, sizeof(xpsr_buf), "xPSR ERROR");

    printf("  │  %-24s   %-25s │\n", sp_buf, lr_buf);
    printf("  │  %-24s   %-25s │\n", pc_buf, xpsr_buf);

    printf(UI_CYAN);
    printf("  └────────────────────────────────────────────────────────┘\n");
    printf(UI_RESET "\n");
}

void ui_breakpoints_view(const ui_breakpoint_info_t *bps, uint8_t count)
{
    printf("\n");
    printf(UI_CYAN UI_BOLD);
    printf("  ▼ HARDWARE BREAKPOINTS (FPB)\n");
    printf("  ┌─────┬──────────┬────────────┬──────────────────────────┐\n");
    printf("  │ NUM │ %-8s │ %-10s │ %-24s │\n", "STATUS", "ADDRESS", "INFO");
    printf("  ├─────┼──────────┼────────────┼──────────────────────────┤\n");
    printf(UI_RESET);

    for (uint8_t i = 0; i < count; i++)
    {
        if (bps[i].enabled)
        {
            printf("  │  %u  │ " UI_GREEN "ENABLED " UI_RESET " │ 0x%08lX │ %-24s │\n",
                   bps[i].index, (unsigned long)bps[i].address, "Active breakpoint");
        }
        else
        {
            printf("  │  %u  │ " UI_DIM "DISABLED" UI_RESET " │ --         │ %-24s │\n",
                   bps[i].index, "Free slot");
        }
    }

    printf(UI_CYAN UI_BOLD);
    printf("  └─────┴──────────┴────────────┴──────────────────────────┘\n");
    printf(UI_RESET "\n");
}
