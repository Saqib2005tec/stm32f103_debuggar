#ifndef STM32F103C6_H
#define STM32F103C6_H

#include <stdint.h>

/* ============================================================
 * Base addresses
 * ============================================================ */

#define PERIPH_BASE        0x40000000UL
#define APB2PERIPH_BASE    (PERIPH_BASE + 0x10000UL)

/* RCC (Located on AHB bus, not APB2) */
#define RCC_BASE           (PERIPH_BASE + 0x21000UL)

/* GPIOC */
#define GPIOC_BASE         (APB2PERIPH_BASE + 0x1000UL)


/* ============================================================
 * RCC registers
 * ============================================================ */

#define RCC_APB2ENR        (*(volatile uint32_t *)(RCC_BASE + 0x18UL))


/* ============================================================
 * GPIO registers
 * ============================================================ */

#define GPIOC_CRL          (*(volatile uint32_t *)(GPIOC_BASE + 0x00UL))
#define GPIOC_CRH          (*(volatile uint32_t *)(GPIOC_BASE + 0x04UL))

#define GPIOC_IDR          (*(volatile uint32_t *)(GPIOC_BASE + 0x08UL))
#define GPIOC_ODR          (*(volatile uint32_t *)(GPIOC_BASE + 0x0CUL))
#define GPIOC_BSRR         (*(volatile uint32_t *)(GPIOC_BASE + 0x10UL))
#define GPIOC_BRR          (*(volatile uint32_t *)(GPIOC_BASE + 0x14UL))
#define GPIOC_LCKR         (*(volatile uint32_t *)(GPIOC_BASE + 0x18UL))


/* ============================================================
 * RCC APB2ENR bits
 * ============================================================ */

/*
 * IOPCEN:
 * I/O port C clock enable
 *
 * Bit 4 = 1 -> GPIOC clock enabled
 */

#define RCC_APB2ENR_IOPCEN     (1UL << 4)


/* ============================================================
 * LED
 * ============================================================ */

#define LED_PIN                13U
#define LED_MASK               (1UL << LED_PIN)


/* ============================================================
 * GPIO configuration
 *
 * STM32F1 GPIO configuration:
 *
 * CNF[1:0] MODE[1:0]
 *
 * Output push-pull, 2 MHz:
 *
 * CNF = 00
 * MODE = 10
 *
 * Therefore:
 *
 * 0010b = 0x2
 * ============================================================ */

#define GPIO_OUTPUT_PP_2MHZ    0x2UL


#endif
