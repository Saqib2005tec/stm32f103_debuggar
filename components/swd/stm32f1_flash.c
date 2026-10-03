#include <stdio.h>
#include "swd_log.h"
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "stm32f1_flash.h"
#include "swd.h"
#include "swd_ap.h"

bool flash_read_register(uint32_t address, uint32_t *value)
{
    if (value == NULL)
        return false;

    return ap_memory_read32(address, value);
}

bool stm32f1_flash_unlock(void)
{
    uint32_t cr;

    /*
     * Read current FLASH_CR
     */
    if (!flash_read_register(FLASH_CR, &cr))
    {
        SWD_LOG("ERROR: Failed to read FLASH_CR\n");
        return false;
    }

    /*
     * Already unlocked
     */
    if ((cr & FLASH_CR_LOCK) == 0)
    {
        SWD_LOG("Flash is already unlocked.\n");
        return true;
    }

    SWD_LOG("Flash is locked. Unlocking...\n");

    /*
     * KEY1
     */
    if (!ap_memory_write32(
            FLASH_KEYR,
            FLASH_KEY1))
    {
        SWD_LOG("ERROR: Failed to write FLASH KEY1\n");
        return false;
    }

    /*
     * KEY2
     */
    if (!ap_memory_write32(
            FLASH_KEYR,
            FLASH_KEY2))
    {
        SWD_LOG("ERROR: Failed to write FLASH KEY2\n");
        return false;
    }

    /*
     * Read FLASH_CR again
     */
    if (!flash_read_register(FLASH_CR, &cr))
    {
        SWD_LOG("ERROR: Failed to read FLASH_CR after unlock\n");
        return false;
    }

    SWD_LOG(
        "FLASH_CR after unlock = 0x%08lX\n",
        (unsigned long)cr
    );

    /*
     * LOCK must now be cleared.
     */
    if (cr & FLASH_CR_LOCK)
    {
        SWD_LOG("ERROR: Flash unlock failed. LOCK is still set.\n");
        return false;
    }

    SWD_LOG("Flash unlocked successfully.\n");

    return true;
}



bool stm32f1_flash_erase_page(uint32_t page_address)
{
    uint32_t cr;
    uint32_t sr;

    /*
     * -------------------------------------------------
     * Validate page address
     * -------------------------------------------------
     */

    if (page_address < STM32F1_FLASH_START)
    {
        SWD_LOG(
            "FLASH ERASE ERROR: Invalid address 0x%08lX\n",
            (unsigned long)page_address
        );

        return false;
    }

    /*
     * STM32F1 page size = 1 KB.
     * Page address must therefore be 1 KB aligned.
     */
    if (page_address & (STM32F1_FLASH_PAGE_SIZE - 1U))
    {
        SWD_LOG(
            "FLASH ERASE ERROR: Address is not page aligned: "
            "0x%08lX\n",
            (unsigned long)page_address
        );

        return false;
    }

    /*
     * -------------------------------------------------
     * Check Flash controller status
     * -------------------------------------------------
     */

    if (!flash_read_register(FLASH_SR, &sr))
    {
        SWD_LOG("FLASH ERASE ERROR: Failed to read FLASH_SR\n");
        return false;
    }

    if (sr & FLASH_SR_BSY)
    {
        SWD_LOG("FLASH ERASE ERROR: Flash is busy\n");
        return false;
    }

    /*
     * -------------------------------------------------
     * Check Flash lock state
     * -------------------------------------------------
     */

    if (!flash_read_register(FLASH_CR, &cr))
    {
        SWD_LOG("FLASH ERASE ERROR: Failed to read FLASH_CR\n");
        return false;
    }

    if (cr & FLASH_CR_LOCK)
    {
        SWD_LOG("FLASH ERASE ERROR: Flash is locked\n");
        return false;
    }

    SWD_LOG(
        "Erasing Flash page at 0x%08lX...\n",
        (unsigned long)page_address
    );

    /*
     * -------------------------------------------------
     * Set Page Erase bit
     * -------------------------------------------------
     */

    cr |= FLASH_CR_PER;

    if (!ap_memory_write32(FLASH_CR, cr))
    {
        SWD_LOG(
            "FLASH ERASE ERROR: Failed to set PER\n"
        );

        return false;
    }

    /*
     * -------------------------------------------------
     * Set page address
     * -------------------------------------------------
     */

    if (!ap_memory_write32(
            FLASH_AR,
            page_address))
    {
        SWD_LOG(
            "FLASH ERASE ERROR: Failed to write FLASH_AR\n"
        );

        /*
         * Disable PER before returning.
         */
        cr &= ~FLASH_CR_PER;
        ap_memory_write32(FLASH_CR, cr);

        return false;
    }

    /*
     * -------------------------------------------------
     * Start erase
     * -------------------------------------------------
     */

    cr |= FLASH_CR_STRT;

    if (!ap_memory_write32(FLASH_CR, cr))
    {
        SWD_LOG(
            "FLASH ERASE ERROR: Failed to start erase\n"
        );

        cr &= ~FLASH_CR_PER;
        ap_memory_write32(FLASH_CR, cr);

        return false;
    }

    /*
     * -------------------------------------------------
     * Wait for BSY to clear
     * -------------------------------------------------
     */

    const int max_polls = 1000;

    for (int i = 0; i < max_polls; i++)
    {
        if (!flash_read_register(FLASH_SR, &sr))
        {
            SWD_LOG(
                "FLASH ERASE ERROR: Failed to read FLASH_SR\n"
            );

            return false;
        }

        if ((sr & FLASH_SR_BSY) == 0)
        {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(1));

        if (i == (max_polls - 1))
        {
            SWD_LOG(
                "FLASH ERASE ERROR: Timeout waiting for BSY\n"
            );

            return false;
        }
    }

    /*
     * -------------------------------------------------
     * Check Flash errors
     * -------------------------------------------------
     */

    if (sr & FLASH_SR_PGERR)
    {
        SWD_LOG(
            "FLASH ERASE ERROR: PGERR set. SR = 0x%08lX\n",
            (unsigned long)sr
        );

        return false;
    }

    if (sr & FLASH_SR_WRPRTERR)
    {
        SWD_LOG(
            "FLASH ERASE ERROR: WRPRTERR set. SR = 0x%08lX\n",
            (unsigned long)sr
        );

        return false;
    }

    /*
     * -------------------------------------------------
     * Check End Of Operation
     * -------------------------------------------------
     */

    if ((sr & FLASH_SR_EOP) == 0)
    {
        SWD_LOG(
            "FLASH ERASE ERROR: EOP not set. SR = 0x%08lX\n",
            (unsigned long)sr
        );

        return false;
    }

    /*
     * -------------------------------------------------
     * Clear EOP
     *
     * STM32F1 clears EOP by writing 1 to it.
     * -------------------------------------------------
     */

    if (!ap_memory_write32(
            FLASH_SR,
            FLASH_SR_EOP))
    {
        SWD_LOG(
            "FLASH ERASE WARNING: Failed to clear EOP\n"
        );
    }

    /*
     * -------------------------------------------------
     * Clear PER
     * -------------------------------------------------
     */

    cr &= ~FLASH_CR_PER;

    if (!ap_memory_write32(FLASH_CR, cr))
    {
        SWD_LOG(
            "FLASH ERASE ERROR: Failed to clear PER\n"
        );

        return false;
    }

    SWD_LOG("Flash page erase completed successfully.\n");

    return true;
}


bool stm32f1_flash_verify_erased_page(uint32_t page_address)
{
    uint32_t value;

    for (uint32_t offset = 0;
         offset < STM32F1_FLASH_PAGE_SIZE;
         offset += 4)
    {
        if (!ap_memory_read32(
                page_address + offset,
                &value))
        {
            SWD_LOG(
                "VERIFY ERROR: Failed at 0x%08lX\n",
                (unsigned long)(page_address + offset)
            );

            return false;
        }

        if (value != 0xFFFFFFFFUL)
        {
            SWD_LOG(
                "VERIFY ERROR: 0x%08lX = 0x%08lX\n",
                (unsigned long)(page_address + offset),
                (unsigned long)value
            );

            return false;
        }
    }

    return true;
}

bool stm32f1_flash_program_halfword(
    uint32_t address,
    uint16_t data
)
{
    uint32_t cr;
    uint32_t sr;

    /*
     * -------------------------------------------------
     * Validate address
     * -------------------------------------------------
     */

    if (address < STM32F1_FLASH_START)
    {
        SWD_LOG(
            "FLASH PROGRAM ERROR: Invalid address "
            "0x%08lX\n",
            (unsigned long)address
        );

        return false;
    }

    /*
     * Flash programming must be half-word aligned.
     */
    if (address & 0x1U)
    {
        SWD_LOG(
            "FLASH PROGRAM ERROR: Unaligned address "
            "0x%08lX\n",
            (unsigned long)address
        );

        return false;
    }

    /*
     * -------------------------------------------------
     * Read FLASH_CR
     * -------------------------------------------------
     */

    if (!flash_read_register(FLASH_CR, &cr))
    {
        SWD_LOG(
            "FLASH PROGRAM ERROR: Failed to read FLASH_CR\n"
        );

        return false;
    }

    /*
     * Flash must be unlocked.
     */
    if (cr & FLASH_CR_LOCK)
    {
        SWD_LOG(
            "FLASH PROGRAM ERROR: Flash is locked\n"
        );

        return false;
    }

    /*
     * -------------------------------------------------
     * Check Flash is not busy
     * -------------------------------------------------
     */

    if (!flash_read_register(FLASH_SR, &sr))
    {
        SWD_LOG(
            "FLASH PROGRAM ERROR: Failed to read FLASH_SR\n"
        );

        return false;
    }

    if (sr & FLASH_SR_BSY)
    {
        SWD_LOG(
            "FLASH PROGRAM ERROR: Flash is busy\n"
        );

        return false;
    }

    /*
     * -------------------------------------------------
     * Enable programming mode
     * -------------------------------------------------
     */

    cr |= FLASH_CR_PG;

    if (!ap_memory_write32(
            FLASH_CR,
            cr))
    {
        SWD_LOG(
            "FLASH PROGRAM ERROR: "
            "Failed to set PG\n"
        );

        return false;
    }

    /*
     * -------------------------------------------------
     * Write 16-bit half-word
     * -------------------------------------------------
     */

    SWD_LOG(
        "Programming 0x%04X at 0x%08lX...\n",
        data,
        (unsigned long)address
    );

    if (!ap_memory_write16(
            address,
            data))
    {
        SWD_LOG(
            "FLASH PROGRAM ERROR: "
            "16-bit Flash write failed\n"
        );

        /*
         * Disable PG before returning.
         */
        cr &= ~FLASH_CR_PG;
        ap_memory_write32(FLASH_CR, cr);

        return false;
    }

    /*
     * -------------------------------------------------
     * Wait for programming operation to complete
     * -------------------------------------------------
     */

    const int max_polls = 1000;

    bool completed = false;

    for (int i = 0; i < max_polls; i++)
    {
        if (!flash_read_register(
                FLASH_SR,
                &sr))
        {
            SWD_LOG(
                "FLASH PROGRAM ERROR: "
                "Failed to read FLASH_SR\n"
            );

            cr &= ~FLASH_CR_PG;
            ap_memory_write32(FLASH_CR, cr);

            return false;
        }

        if ((sr & FLASH_SR_BSY) == 0)
        {
            completed = true;
            break;
        }

        vTaskDelay(
            pdMS_TO_TICKS(1)
        );
    }

    if (!completed)
    {
        SWD_LOG(
            "FLASH PROGRAM ERROR: "
            "Timeout waiting for BSY\n"
        );

        cr &= ~FLASH_CR_PG;
        ap_memory_write32(FLASH_CR, cr);

        return false;
    }

    /*
     * -------------------------------------------------
     * Check programming errors
     * -------------------------------------------------
     */

    if (sr & FLASH_SR_PGERR)
    {
        SWD_LOG(
            "FLASH PROGRAM ERROR: PGERR "
            "SR = 0x%08lX\n",
            (unsigned long)sr
        );

        cr &= ~FLASH_CR_PG;
        ap_memory_write32(FLASH_CR, cr);

        return false;
    }

    if (sr & FLASH_SR_WRPRTERR)
    {
        SWD_LOG(
            "FLASH PROGRAM ERROR: WRPRTERR "
            "SR = 0x%08lX\n",
            (unsigned long)sr
        );

        cr &= ~FLASH_CR_PG;
        ap_memory_write32(FLASH_CR, cr);

        return false;
    }

    /*
     * -------------------------------------------------
     * Check EOP
     * -------------------------------------------------
     */

    if ((sr & FLASH_SR_EOP) == 0)
    {
        SWD_LOG(
            "FLASH PROGRAM ERROR: "
            "EOP not set. SR = 0x%08lX\n",
            (unsigned long)sr
        );

        cr &= ~FLASH_CR_PG;
        ap_memory_write32(FLASH_CR, cr);

        return false;
    }

    /*
     * -------------------------------------------------
     * Clear EOP
     * -------------------------------------------------
     */

    if (!ap_memory_write32(
            FLASH_SR,
            FLASH_SR_EOP))
    {
        SWD_LOG(
            "FLASH PROGRAM WARNING: "
            "Failed to clear EOP\n"
        );
    }

    /*
     * -------------------------------------------------
     * Disable programming mode
     * -------------------------------------------------
     */

    cr &= ~FLASH_CR_PG;

    if (!ap_memory_write32(
            FLASH_CR,
            cr))
    {
        SWD_LOG(
            "FLASH PROGRAM ERROR: "
            "Failed to clear PG\n"
        );

        return false;
    }

    return true;
}

bool stm32f1_flash_program_buffer(
    uint32_t address,
    const uint8_t *data,
    uint32_t length
)
{
    if (data == NULL)
    {
        SWD_LOG(
            "FLASH BUFFER PROGRAM ERROR: "
            "NULL data pointer\n"
        );

        return false;
    }

    if (length == 0)
    {
        SWD_LOG(
            "FLASH BUFFER PROGRAM ERROR: "
            "Zero length\n"
        );

        return false;
    }

    if (address < STM32_FLASH_START)
{
    SWD_LOG(
        "FLASH BUFFER PROGRAM ERROR: "
        "Invalid address 0x%08lX\n",
        (unsigned long)address
    );

    return false;
}

if (length >
    (STM32_FLASH_END - address + 1UL))
{
    SWD_LOG(
        "FLASH BUFFER PROGRAM ERROR: "
        "Image exceeds Flash size\n"
    );

    return false;
}

    /*
     * Flash programming requires half-word
     * aligned destination.
     */
    if (address & 0x1U)
    {
        SWD_LOG(
            "FLASH BUFFER PROGRAM ERROR: "
            "Unaligned address 0x%08lX\n",
            (unsigned long)address
        );

        return false;
    }

    /*
     * For now, require an even number of bytes.
     *
     * We will later support odd-sized .bin files
     * by padding the final byte with 0xFF.
     */
    if (length & 0x1U)
    {
        SWD_LOG(
            "FLASH BUFFER PROGRAM ERROR: "
            "Length must be even (%lu)\n",
            (unsigned long)length
        );

        return false;
    }

    /*
     * Make sure the Flash is unlocked.
     */
    uint32_t cr;

    if (!flash_read_register(
            FLASH_CR,
            &cr))
    {
        SWD_LOG(
            "FLASH BUFFER PROGRAM ERROR: "
            "Failed to read FLASH_CR\n"
        );

        return false;
    }

    if (cr & FLASH_CR_LOCK)
    {
        SWD_LOG(
            "FLASH BUFFER PROGRAM ERROR: "
            "Flash is locked\n"
        );

        return false;
    }

    /*
     * Program each half-word.
     */
    for (uint32_t i = 0; i < length; i += 2)
    {
        uint16_t halfword =
            (uint16_t)data[i] |
            ((uint16_t)data[i + 1] << 8);

        uint32_t target_address =
            address + i;

        if (!stm32f1_flash_program_halfword(
                target_address,
                halfword))
        {
            SWD_LOG(
                "FLASH BUFFER PROGRAM ERROR: "
                "Failed at offset 0x%lX\n",
                (unsigned long)i
            );

            return false;
        }

        if ((i % 16U) == 0U)
{
    vTaskDelay(1);
}
    }

    return true;
}



bool stm32f1_flash_mass_erase(void)
{
    uint32_t cr;
    uint32_t sr;
    uint32_t timeout = 0;

    SWD_LOG("\n");
    SWD_LOG("========================================\n");
    SWD_LOG(" STM32F1 FLASH MASS ERASE\n");
    SWD_LOG("========================================\n");

    /*
     * 1. Unlock Flash controller
     */
    if (!stm32f1_flash_unlock()) {
        printf("\n  \033[31m✗ Flash unlock failed!\033[0m\n");
        return false;
    }

    /*
     * 2. Clear any lingering error flags in FLASH_SR (PGERR, WRPRTERR, EOP)
     */
    if (ap_memory_read32(FLASH_SR, &sr)) {
        if (sr & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR | FLASH_SR_EOP)) {
            ap_memory_write32(FLASH_SR, sr & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR | FLASH_SR_EOP));
        }
    }

    /*
     * 3. Wait until Flash is not busy
     */
    timeout = 0;
    do {
        if (!ap_memory_read32(FLASH_SR, &sr)) {
            printf("\n  \033[31m✗ Cannot read FLASH_SR\033[0m\n");
            return false;
        }

        if (++timeout >= FLASH_ERASE_TIMEOUT) {
            printf("\n  \033[31m✗ Timeout waiting for Flash ready\033[0m\n");
            return false;
        }
    } while (sr & FLASH_SR_BSY);

    /*
     * 4. Clear other mode bits (PG, PER) and set MER
     */
    if (!ap_memory_read32(FLASH_CR, &cr)) {
        printf("\n  \033[31m✗ Cannot read FLASH_CR\033[0m\n");
        return false;
    }

    cr &= ~(FLASH_CR_PG | FLASH_CR_PER);
    cr |= FLASH_CR_MER;

    if (!ap_memory_write32(FLASH_CR, cr)) {
        printf("\n  \033[31m✗ Failed to set MER in FLASH_CR\033[0m\n");
        return false;
    }

    /*
     * 5. Start erase (set STRT bit while keeping MER set)
     */
    if (!ap_memory_write32(FLASH_CR, cr | FLASH_CR_STRT)) {
        printf("\n  \033[31m✗ Failed to start mass erase\033[0m\n");
        return false;
    }

    /*
     * 6. Critical: Wait for charge pump to engage and BSY to be asserted.
     * STM32F1 Flash mass erase takes ~30-40ms. If we poll immediately,
     * BSY may still read 0, causing premature exit and aborting the erase.
     */
    vTaskDelay(pdMS_TO_TICKS(10));

    /*
     * 7. Wait until BSY becomes 0
     */
    timeout = 0;
    while (1) {
        if (!ap_memory_read32(FLASH_SR, &sr)) {
            printf("\n  \033[31m✗ Cannot read FLASH_SR during erase\033[0m\n");
            return false;
        }

        if (!(sr & FLASH_SR_BSY)) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(5));

        if (++timeout >= 300) { /* 1500 ms timeout */
            printf("\n  \033[31m✗ Flash mass erase timeout! (SR=0x%08lX)\033[0m\n", (unsigned long)sr);
            return false;
        }
    }

    /*
     * 8. Check for errors
     */
    if (sr & FLASH_SR_WRPRTERR) {
        printf("\n  \033[31m✗ Mass erase error: WRPRTERR (Flash is write-protected)\033[0m\n");
        return false;
    }

    if (sr & FLASH_SR_PGERR) {
        printf("\n  \033[31m✗ Mass erase error: PGERR\033[0m\n");
        return false;
    }

    /*
     * 9. Clear EOP by writing 1 to FLASH_SR
     */
    if (sr & FLASH_SR_EOP) {
        ap_memory_write32(FLASH_SR, FLASH_SR_EOP);
    }

    /*
     * 10. Clear MER
     */
    if (!ap_memory_read32(FLASH_CR, &cr)) {
        return false;
    }

    cr &= ~FLASH_CR_MER;

    if (!ap_memory_write32(FLASH_CR, cr)) {
        return false;
    }

    /* Settling delay after erase completion */
    vTaskDelay(pdMS_TO_TICKS(20));

    return true;
}


bool stm32f1_flash_verify_erased_with_progress(
    stm32f1_verify_progress_cb_t progress_cb,
    void *user_data
)
{
    uint32_t value;
    const uint32_t total_bytes = STM32_FLASH_SIZE;
    uint32_t verified_words = 0;

    for (uint32_t address = STM32_FLASH_START;
         address <= STM32_FLASH_END;
         address += sizeof(uint32_t))
    {
        if (!ap_memory_read32(address, &value))
        {
            printf("\n  \033[31m✗ Flash verify read failed at 0x%08lX (SWD: %s)\033[0m\n",
                   (unsigned long)address, swd_status_string(swd_get_last_status()));
            return false;
        }

        if (value != 0xFFFFFFFFUL)
        {
            printf("\n  \033[31m✗ Flash not erased at 0x%08lX! Read: 0x%08lX (expected 0xFFFFFFFF)\033[0m\n",
                   (unsigned long)address, (unsigned long)value);
            return false;
        }

        verified_words++;

        /*
         * Delay 1 tick every 32 words (~12ms of work) to ensure the FreeRTOS
         * IDLE task and Task Watchdog get time, and the SWD/SPI bus remains stable.
         * 256 delays across 32 KB reduces verify time from ~6s to ~2.5s.
         */
        if ((verified_words % 32U) == 0U)
        {
            vTaskDelay(1);
        }

        /*
         * Notify progress callback every 32 words (128 bytes)
         */
        if (progress_cb != NULL && ((verified_words % 32U) == 0U))
        {
            uint32_t verified_bytes = verified_words * sizeof(uint32_t);
            progress_cb(address, verified_bytes, total_bytes, user_data);
        }
    }

    if (progress_cb != NULL)
    {
        progress_cb(STM32_FLASH_END, total_bytes, total_bytes, user_data);
    }

    return true;
}

bool stm32f1_flash_verify_erased(void)
{
    return stm32f1_flash_verify_erased_with_progress(NULL, NULL);
}



#include "stm32f1_flash.h"

#include <stdio.h>
#include <stddef.h>

bool stm32f1_validate_bin(
    const uint8_t *data,
    size_t size)
{
    if (data == NULL) {
        SWD_LOG("ERROR: Firmware data is NULL.\n");
        return false;
    }

    if (size < 8U) {
        SWD_LOG("ERROR: Firmware image is too small.\n");
        return false;
    }

    uint32_t initial_sp =
        ((uint32_t)data[0])       |
        ((uint32_t)data[1] << 8)  |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);

    uint32_t reset_handler =
        ((uint32_t)data[4])       |
        ((uint32_t)data[5] << 8)  |
        ((uint32_t)data[6] << 16) |
        ((uint32_t)data[7] << 24);

    SWD_LOG("\n");
    SWD_LOG("========================================\n");
    SWD_LOG(" STM32F1 BIN IMAGE\n");
    SWD_LOG("========================================\n");

    SWD_LOG("Image size     : %lu bytes\n",
           (unsigned long)size);

    SWD_LOG("Initial MSP    : 0x%08lX\n",
           (unsigned long)initial_sp);

    SWD_LOG("Reset Handler  : 0x%08lX\n",
           (unsigned long)reset_handler);

    /*
     * STM32F103C6 SRAM:
     *
     * 0x20000000 - 0x20004FFF
     */
    if (initial_sp < 0x20000000UL ||
        initial_sp > 0x20005000UL)
    {
        SWD_LOG("ERROR: Invalid initial MSP.\n");
        return false;
    }

    /*
     * STM32F103C6 Flash:
     *
     * 0x08000000 - 0x08007FFF
     */
    if (reset_handler < 0x08000000UL ||
        reset_handler >= 0x08008000UL)
    {
        SWD_LOG("ERROR: Invalid reset handler.\n");
        return false;
    }

    /*
     * Cortex-M vector table entries contain Thumb
     * addresses, therefore bit 0 should normally be 1.
     */
    if ((reset_handler & 1U) == 0U)
    {
        SWD_LOG("ERROR: Reset handler is not a Thumb address.\n");
        return false;
    }

    SWD_LOG("BIN image validation PASSED.\n");

    return true;
}



bool stm32f1_flash_verify_buffer(
    uint32_t address,
    const uint8_t *data,
    uint32_t length
)
{
    if (data == NULL)
    {
        SWD_LOG(
            "FLASH VERIFY ERROR: NULL data pointer\n"
        );

        return false;
    }

    if (length == 0U)
    {
        SWD_LOG(
            "FLASH VERIFY ERROR: Zero length\n"
        );

        return false;
    }

    /*
     * Check Flash address range.
     */
    if (address < STM32_FLASH_START)
    {
        SWD_LOG(
            "FLASH VERIFY ERROR: Invalid start address "
            "0x%08lX\n",
            (unsigned long)address
        );

        return false;
    }

    if (length >
        (STM32_FLASH_END - address + 1UL))
    {
        SWD_LOG(
            "FLASH VERIFY ERROR: Image exceeds Flash\n"
        );

        return false;
    }

    uint32_t verified = 0;

 for (uint32_t i = 0;
     i < length;
     i++)
{
    uint32_t actual_word;

    if (!ap_memory_read(
            address + i,
            &actual_word,
            AP_ACCESS_8BIT))
    {
        SWD_LOG(
            "FLASH VERIFY ERROR: "
            "Read failed at 0x%08lX\n",
            (unsigned long)(address + i)
        );

        return false;
    }

    uint8_t actual =
        (uint8_t)actual_word;

    if (actual != data[i])
    {
        SWD_LOG(
            "FLASH VERIFY ERROR:\n"
            "Address : 0x%08lX\n"
            "Expected: 0x%02X\n"
            "Actual  : 0x%02X\n",
            (unsigned long)(address + i),
            data[i],
            actual
        );

        return false;
    }

    verified++;

    if ((verified % 16U) == 0U)
    {
        vTaskDelay(1);
    }
}

    return true;
}