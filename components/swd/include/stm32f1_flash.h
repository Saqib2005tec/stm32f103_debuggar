#ifndef STM32F1_FLASH_H
#define STM32F1_FLASH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
/* STM32F1 Flash controller base address */
#define STM32F1_FLASH_BASE       0x40022000UL

/* Flash registers */
#define FLASH_ACR                (STM32F1_FLASH_BASE + 0x00UL)
#define FLASH_KEYR               (STM32F1_FLASH_BASE + 0x04UL)
#define FLASH_OPTKEYR            (STM32F1_FLASH_BASE + 0x08UL)
#define FLASH_SR                 (STM32F1_FLASH_BASE + 0x0CUL)
#define FLASH_CR                 (STM32F1_FLASH_BASE + 0x10UL)
#define FLASH_AR                 (STM32F1_FLASH_BASE + 0x14UL)
#define FLASH_OBR                (STM32F1_FLASH_BASE + 0x1CUL)
#define FLASH_WRPR               (STM32F1_FLASH_BASE + 0x20UL)

/* Flash status register bits */
#define FLASH_SR_BSY             (1UL << 0)
#define FLASH_SR_PG              (1UL << 2)
#define FLASH_SR_WRPRTERR        (1UL << 4)
#define FLASH_SR_EOP             (1UL << 5)

/* Flash control register bits */
#define FLASH_CR_PG              (1UL << 0)
#define FLASH_CR_PER             (1UL << 1)
#define FLASH_CR_MER             (1UL << 2)
#define FLASH_CR_OPTPG           (1UL << 4)
#define FLASH_CR_OPTER           (1UL << 5)
#define FLASH_CR_STRT            (1UL << 6)
#define FLASH_CR_LOCK            (1UL << 7)

/* STM32F1 Flash unlock keys */
#define FLASH_KEY1               0x45670123UL
#define FLASH_KEY2               0xCDEF89ABUL

#define FLASH_SR_BSY             (1UL << 0)
#define FLASH_SR_PG              (1UL << 2)
#define FLASH_SR_WRPRTERR        (1UL << 4)
#define FLASH_SR_EOP             (1UL << 5)

#define FLASH_SR_PGERR           (1UL << 2)


#define STM32F1_FLASH_PAGE_SIZE  0x400UL

#define STM32F1_FLASH_START      0x08000000UL


#define STM32_FLASH_START       0x08000000UL
#define STM32_FLASH_SIZE        (32UL * 1024UL)
#define STM32_FLASH_END         \
    (STM32_FLASH_START + STM32_FLASH_SIZE - 1UL)

#define FLASH_ERASE_TIMEOUT     1000000UL



bool flash_read_register(uint32_t address, uint32_t *value);

bool stm32f1_flash_unlock(void);

bool stm32f1_flash_erase_page(uint32_t page_address);

bool stm32f1_flash_verify_erased_page(
    uint32_t page_address
);

bool stm32f1_flash_program_halfword(
    uint32_t address,
    uint16_t data
);

bool stm32f1_flash_program_buffer(
    uint32_t address,
    const uint8_t *data,
    uint32_t length
);


typedef void (*stm32f1_verify_progress_cb_t)(
    uint32_t current_address,
    uint32_t verified_bytes,
    uint32_t total_bytes,
    void *user_data
);

bool stm32f1_flash_mass_erase(void);
bool stm32f1_flash_verify_erased(void);
bool stm32f1_flash_verify_erased_with_progress(
    stm32f1_verify_progress_cb_t progress_cb,
    void *user_data
);

bool stm32f1_validate_bin(
    const uint8_t *data,
    size_t size
);


bool stm32f1_flash_verify_buffer(
    uint32_t address,
    const uint8_t *data,
    uint32_t length
);


#endif