#ifndef SWD_TEST_H
#define SWD_TEST_H

#include <stdbool.h>
#include <stdint.h>
/*
 * =========================================================
 * SWD / Debugger Test Suite
 * =========================================================
 *
 * All hardware validation tests are declared here.
 */


/*
 * Basic AHB-AP memory tests
 */
bool swd_test_flash_read(void);

bool swd_test_generic_memory_read(void);


/*
 * Cortex-M3 debug tests
 */
bool swd_test_cortex_m3_debug(void);


/*
 * STM32F1 Flash controller tests
 */
bool swd_test_flash_controller(void);

bool swd_test_flash_page_erase(void);

bool swd_test_flash_program_buffer(void);

bool swd_test_flash_read16(void);


/*
 * Complete test suite
 */
bool swd_run_all_tests(void);



#endif /* SWD_TEST_H */