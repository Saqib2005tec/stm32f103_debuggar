#include "stm32f103c6.h"

/*
 * Simple software delay.
 *
 * This is intentionally crude.
 * Later you can replace this with the SysTick timer.
 */
static void delay(volatile uint32_t count)
{
    while (count--)
    {
        __asm volatile ("nop");
    }
}


/*
 * Configure PC13 as:
 *
 * Output
 * Push-pull
 * 2 MHz
 */
static void led_init(void)
{
    /*
     * Enable GPIOC peripheral clock.
     *
     * RCC_APB2ENR bit 4 = IOPCEN
     */
    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;


    /*
     * PC13 is located in CRH because:
     *
     * PC0-PC7  -> CRL
     * PC8-PC15 -> CRH
     *
     * PC13 position inside CRH:
     *
     * (13 - 8) * 4
     * = 5 * 4
     * = 20
     */

    GPIOC_CRH &= ~(0xFUL << 20);

    /*
     * MODE = 10
     * CNF  = 00
     *
     * Output push-pull, 2 MHz
     */
    GPIOC_CRH |= (GPIO_OUTPUT_PP_2MHZ << 20);
}


/*
 * Turn LED ON.
 *
 * Blue Pill PC13 LED is normally active-low.
 */
static void __attribute__((noinline)) led_on(void)
{
    GPIOC_BRR = LED_MASK;
}


/*
 * Turn LED OFF.
 */
static void __attribute__((noinline)) led_off(void)
{
    GPIOC_BSRR = LED_MASK;
}


int main(void)
{
    led_init();

    while (1)
    {
        led_on();

        delay(10);

        led_off();

        delay(10);
    }

    /*
     * Should never reach here.
     */
    return 0;
}
