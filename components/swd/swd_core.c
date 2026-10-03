#include "swd_core.h"
#include "swd_ap.h"

#include <stdio.h>
#include "swd_log.h"


/*
 * ---------------------------------------------------------
 * Read DHCSR
 * ---------------------------------------------------------
 */
uint32_t core_read_dhcsr(void)
{
    uint32_t value = 0;

    if (!ap_memory_read32(DHCSR, &value)) {
        SWD_LOG("Failed to read DHCSR\n");
        return 0;
    }

    return value;
}


/*
 * ---------------------------------------------------------
 * Enable debug
 *
 * C_DEBUGEN = 1
 * ---------------------------------------------------------
 */
bool core_debug_enable(void)
{
    uint32_t value =
        DHCSR_DBGKEY << 16 |
        DHCSR_C_DEBUGEN;

    return ap_memory_write32(
        DHCSR,
        value
    );
}


/*
 * ---------------------------------------------------------
 * Halt Cortex-M3
 *
 * C_DEBUGEN = 1
 * C_HALT    = 1
 *
 * Value:
 *
 * 0xA05F0003
 * ---------------------------------------------------------
 */
bool core_halt(void)
{
    uint32_t value =
        (DHCSR_DBGKEY << 16) |
        DHCSR_C_DEBUGEN |
        DHCSR_C_HALT;

    SWD_LOG(
        "Writing DHCSR = 0x%08lX\n",
        (unsigned long)value
    );

    if (!ap_memory_write32(DHCSR, value))
    {
        SWD_LOG("Failed to write DHCSR for HALT\n");
        return false;
    }

    for (int i = 0; i < 1000; i++)
    {
        if (core_is_halted())
        {
            return true;
        }
    }

    return false;
}


/*
 * ---------------------------------------------------------
 * Resume Cortex-M3
 *
 * C_DEBUGEN = 1
 * C_HALT    = 0
 *
 * Value:
 *
 * 0xA05F0001
 * ---------------------------------------------------------
 */
bool core_resume(void)
{
    uint32_t value =
        (DHCSR_DBGKEY << 16) |
        DHCSR_C_DEBUGEN;

    if (!ap_memory_write32(
            DHCSR,
            value))
    {
        SWD_LOG("Failed to write DHCSR for RESUME\n");
        return false;
    }

    for (int i = 0; i < 1000; i++)
    {
        if (!core_is_halted())
        {
            return true;
        }
    }

    return true;
}


/*
 * ---------------------------------------------------------
 * Step Cortex-M3 (Single instruction)
 *
 * C_DEBUGEN  = 1
 * C_MASKINTS = 1
 * C_STEP     = 1
 * C_HALT     = 0
 * ---------------------------------------------------------
 */
bool core_step(void)
{
    if (!core_is_halted())
    {
        SWD_LOG("CPU is not halted. Cannot single step.\n");
        return false;
    }

    uint32_t value =
        (DHCSR_DBGKEY << 16) |
        DHCSR_C_DEBUGEN |
        DHCSR_C_MASKINTS |
        DHCSR_C_STEP;

    if (!ap_memory_write32(DHCSR, value))
    {
        SWD_LOG("Failed to write DHCSR for STEP\n");
        return false;
    }

    for (int i = 0; i < 1000; i++)
    {
        if (core_is_halted())
        {
            uint32_t halt_value =
                (DHCSR_DBGKEY << 16) |
                DHCSR_C_DEBUGEN |
                DHCSR_C_HALT;
            ap_memory_write32(DHCSR, halt_value);
            return true;
        }
    }

    SWD_LOG("Timeout waiting for core to halt after single step\n");
    return false;
}


/*
 * ---------------------------------------------------------
 * Check whether Cortex-M3 is halted
 *
 * S_HALT = bit 17
 * ---------------------------------------------------------
 */
bool core_is_halted(void)
{
    uint32_t dhcsr =
        core_read_dhcsr();

    return
        (dhcsr & DHCSR_S_HALT) != 0;
}


bool core_read_register(
    uint8_t reg_num,
    uint32_t *value
)
{

    if (reg_num > CORE_REG_XPSR)
{
    SWD_LOG(
        "Invalid core register number: %u\n",
        reg_num
    );

    return false;
}
    if (value == NULL)
    {
        return false;
    }

    /*
     * -----------------------------------------------------
     * Step 1: Make sure CPU is halted
     * -----------------------------------------------------
     */

    if (!core_is_halted())
    {
        SWD_LOG(
            "CPU is not halted. Cannot read CPU register.\n"
        );

        return false;
    }


    /*
     * -----------------------------------------------------
     * Step 2: Select CPU register
     * -----------------------------------------------------
     */

    uint32_t dcrsr_value =
        (uint32_t)reg_num;

    if (!ap_memory_write32(
            DCRSR,
            dcrsr_value))
    {
        SWD_LOG(
            "Failed to write DCRSR.\n"
        );

        return false;
    }


    /*
     * -----------------------------------------------------
     * Step 3: Wait for register transfer
     * -----------------------------------------------------
     */

    for (int i = 0; i < 1000; i++)
    {
        uint32_t dhcsr =
            core_read_dhcsr();

        if (dhcsr & DHCSR_S_REGRDY)
        {
            break;
        }

        if (i == 999)
        {
            SWD_LOG(
                "Timeout waiting for S_REGRDY.\n"
            );

            return false;
        }
    }


    /*
     * -----------------------------------------------------
     * Step 4: Read DCRDR
     * -----------------------------------------------------
     */

    if (!ap_memory_read32(
            DCRDR,
            value))
    {
        SWD_LOG(
            "Failed to read DCRDR.\n"
        );

        return false;
    }


    return true;
}


void core_dump_registers(void)
{
    uint32_t value;

    SWD_LOG("\n");
    SWD_LOG("=============================\n");
    SWD_LOG(" CORTEX-M3 REGISTER DUMP\n");
    SWD_LOG("=============================\n");

    for (uint8_t reg = CORE_REG_R0;
         reg <= CORE_REG_R12;
         reg++)
    {
        if (core_read_register(reg, &value))
        {
            SWD_LOG(
                "R%-2u  = 0x%08lX\n",
                reg,
                (unsigned long)value
            );
        }
        else
        {
            SWD_LOG(
                "R%-2u  = READ FAILED\n",
                reg
            );
        }
    }

    /*
     * R13 = SP
     */
    if (core_read_register(CORE_REG_SP, &value))
    {
        SWD_LOG(
            "SP   = 0x%08lX\n",
            (unsigned long)value
        );
    }
    else
    {
        SWD_LOG("SP   = READ FAILED\n");
    }

    /*
     * R14 = LR
     */
    if (core_read_register(CORE_REG_LR, &value))
    {
        SWD_LOG(
            "LR   = 0x%08lX\n",
            (unsigned long)value
        );
    }
    else
    {
        SWD_LOG("LR   = READ FAILED\n");
    }

    /*
     * R15 = PC
     */
    if (core_read_register(CORE_REG_PC, &value))
    {
        SWD_LOG(
            "PC   = 0x%08lX\n",
            (unsigned long)value
        );
    }
    else
    {
        SWD_LOG("PC   = READ FAILED\n");
    }

    /*
     * xPSR
     */
    if (core_read_register(CORE_REG_XPSR, &value))
    {
        SWD_LOG(
            "xPSR = 0x%08lX\n",
            (unsigned long)value
        );
    }
    else
    {
        SWD_LOG("xPSR = READ FAILED\n");
    }

    SWD_LOG("=============================\n");
}


/*
 * =========================================================
 * Hardware Breakpoint Implementation (Cortex-M3 FPB Unit)
 * =========================================================
 */

bool core_bp_enable_unit(bool enable)
{
    /* Ensure DEMCR.TRCENA (bit 24) is set so debug blocks are clocked */
    uint32_t demcr = 0;
    if (ap_memory_read32(DEMCR, &demcr))
    {
        if (!(demcr & (1UL << 24)))
        {
            demcr |= (1UL << 24);
            ap_memory_write32(DEMCR, demcr);
        }
    }

    uint32_t ctrl = FP_CTRL_KEY | (enable ? FP_CTRL_ENABLE : 0UL);
    return ap_memory_write32(FP_CTRL, ctrl);
}

bool core_bp_set(uint8_t index, uint32_t address)
{
    if (index >= FP_MAX_BREAKPOINTS)
    {
        SWD_LOG("Invalid breakpoint index %u\n", index);
        return false;
    }

    /* Enable FPB unit */
    core_bp_enable_unit(true);

    /* Strip Thumb bit 0 */
    address &= ~1UL;

    /*
     * Bits [31:30] REPLACE:
     * 0b01 (1) = Lower halfword match (addr & 2 == 0)
     * 0b10 (2) = Upper halfword match (addr & 2 != 0)
     */
    uint32_t replace = (address & 2UL) ? 2UL : 1UL;
    uint32_t comp_val = (address & 0x1FFFFFFCUL) | (replace << 30) | 1UL;

    return ap_memory_write32(FP_COMP(index), comp_val);
}

bool core_bp_clear(uint8_t index)
{
    if (index >= FP_MAX_BREAKPOINTS)
    {
        return false;
    }

    return ap_memory_write32(FP_COMP(index), 0UL);
}

bool core_bp_clear_all(void)
{
    bool ok = true;
    for (uint8_t i = 0; i < FP_MAX_BREAKPOINTS; i++)
    {
        if (!core_bp_clear(i))
        {
            ok = false;
        }
    }
    return ok;
}

bool core_bp_get(uint8_t index, bool *enabled, uint32_t *address)
{
    if (index >= FP_MAX_BREAKPOINTS || !enabled || !address)
    {
        return false;
    }

    uint32_t val = 0;
    if (!ap_memory_read32(FP_COMP(index), &val))
    {
        return false;
    }

    *enabled = (val & 1UL) != 0;
    if (*enabled)
    {
        uint32_t addr = val & 0x1FFFFFFCUL;
        uint32_t replace = (val >> 30) & 3UL;
        if (replace == 2UL)
        {
            addr |= 2UL;
        }
        *address = addr;
    }
    else
    {
        *address = 0;
    }

    return true;
}

int8_t core_bp_find_free_slot(void)
{
    for (uint8_t i = 0; i < FP_MAX_BREAKPOINTS; i++)
    {
        bool enabled = false;
        uint32_t addr = 0;
        if (core_bp_get(i, &enabled, &addr) && !enabled)
        {
            return (int8_t)i;
        }
    }
    return -1;
}

bool core_bp_is_active_at(uint32_t address)
{
    address &= ~1UL;
    for (uint8_t i = 0; i < FP_MAX_BREAKPOINTS; i++)
    {
        bool enabled = false;
        uint32_t addr = 0;
        if (core_bp_get(i, &enabled, &addr) && enabled)
        {
            if (addr == address)
            {
                return true;
            }
        }
    }
    return false;
}

bool core_is_breakpoint_hit(void)
{
    if (!core_is_halted())
    {
        return false;
    }

    uint32_t dfsr = 0;
    if (ap_memory_read32(DFSR, &dfsr))
    {
        if (dfsr & DFSR_BKPT)
        {
            /* Clear sticky BKPT flag in DFSR by writing 1 */
            ap_memory_write32(DFSR, DFSR_BKPT);
            return true;
        }
    }

    /* Fallback: check if current PC matches any active breakpoint */
    uint32_t pc = 0;
    if (core_read_register(CORE_REG_PC, &pc))
    {
        return core_bp_is_active_at(pc);
    }

    return false;
}

bool core_step_with_stepover(void)
{
    if (!core_is_halted())
    {
        return false;
    }

    uint32_t pc = 0;
    bool res = false;
    if (core_read_register(CORE_REG_PC, &pc))
    {
        if (core_bp_is_active_at(pc))
        {
            /* Temporarily disable FPB unit so we can step past this instruction */
            core_bp_enable_unit(false);

            /* Step one instruction */
            res = core_step();

            /* Re-enable FPB unit */
            core_bp_enable_unit(true);
            
            return res;
        }
    }

    return core_step();
}

bool core_resume_with_stepover(void)
{
    if (!core_is_halted())
    {
        return true;
    }

    uint32_t pc = 0;
    if (core_read_register(CORE_REG_PC, &pc))
    {
        if (core_bp_is_active_at(pc))
        {
            /* Temporarily disable FPB unit so we can step past this instruction */
            core_bp_enable_unit(false);

            /* Step one instruction */
            core_step();

            /* Re-enable FPB unit */
            core_bp_enable_unit(true);
        }
    }

    return core_resume();
}