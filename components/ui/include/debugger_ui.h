#ifndef DEBUGGER_UI_H
#define DEBUGGER_UI_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UI_RESET       "\033[0m"
#define UI_BOLD        "\033[1m"
#define UI_CYAN        "\033[36m"
#define UI_GREEN       "\033[32m"
#define UI_RED         "\033[31m"
#define UI_YELLOW      "\033[33m"
#define UI_DIM         "\033[2m"

void ui_init(void);
void ui_header(void);
void ui_footer_success(uint32_t image_size, uint64_t elapsed_ms);
void ui_footer_failure(const char *reason);

void ui_section(const char *title);
void ui_step_start(const char *label);
void ui_step_ok(void);
void ui_step_fail(const char *reason);

void ui_spinner_start(const char *label);
void ui_spinner_stop(bool success);

void ui_progress(uint32_t current, uint32_t total);
void ui_progress_done(bool success);

void ui_verify_erase_progress(uint32_t address, uint32_t current, uint32_t total);
void ui_verify_erase_done(bool success);

void ui_target_info(const char *target, const char *core);
void ui_firmware_info(uint32_t address, uint32_t size,
                      uint32_t msp, uint32_t reset_handler);

uint64_t ui_time_ms(void);

void ui_menu_show(void);
void ui_menu_prompt(void);
void ui_registers_view(const uint32_t regs[17], const bool valid[17]);

typedef struct {
    uint8_t index;
    bool enabled;
    uint32_t address;
} ui_breakpoint_info_t;

void ui_breakpoints_view(const ui_breakpoint_info_t *bps, uint8_t count);

#ifdef __cplusplus
}
#endif

#endif
