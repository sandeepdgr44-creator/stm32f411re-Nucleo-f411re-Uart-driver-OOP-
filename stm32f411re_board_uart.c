/******************************************************************************
 * @file    stm32f411re_board_uart.c
 * @author  Sandeep Ray
 *
 * @brief
 * Board specific UART implementation for STM32F411RE.
 *
 * This file performs low level hardware configuration required
 * before the generic UART driver can operate.
 *
 * Responsibilities of this module:
 *
 *  - Enable peripheral clocks
 *  - Configure GPIO alternate functions
 *  - Enable NVIC interrupts
 *  - Provide board specific initialization callbacks
 *  - Connect ISR handlers with UART driver layer
 *
 * Design Philosophy
 * -----------------
 * Driver Layer      : generic UART logic
 * Board Layer       : THIS FILE (hardware mapping)
 * Application Layer : uses Serial / Serial1 / Serial6
 *
 * This separation allows the UART driver to remain reusable
 * across multiple STM32 boards.
 *
 ******************************************************************************/

#include "stm32f411re_board_uart.h"


/******************************************************************************
 *                      SYSTEM MEMORY MAP DEFINITIONS
 ******************************************************************************/

/*
 * STM32 peripherals are memory mapped.
 * Base addresses are defined according to
 * STM32F411 reference manual.
 */

#define PERIPH_BASE       (0x40000000UL)

/* Bus regions */
#define AHB1PERIPH_BASE   (PERIPH_BASE + 0x00020000UL)
#define APB1PERIPH_BASE   (PERIPH_BASE + 0x00000000UL)
#define APB2PERIPH_BASE   (PERIPH_BASE + 0x00010000UL)

/* Peripheral base addresses */
#define GPIOA_BASE        (AHB1PERIPH_BASE + 0x0000UL)
#define RCC_BASE          (AHB1PERIPH_BASE + 0x3800UL)


/******************************************************************************
 *                          NVIC REGISTER ACCESS
 ******************************************************************************/

/*
 * NVIC Interrupt Set Enable Registers.
 *
 * Writing '1' enables interrupt.
 * Writing '0' has no effect.
 *
 * Direct register access is used instead of CMSIS
 * to keep driver dependency minimal.
 */

#define NVIC_ISER0        (*(volatile uint32_t *)0xE000E100UL)
#define NVIC_ISER1        (*(volatile uint32_t *)0xE000E104UL)


/******************************************************************************
 *                          IRQ NUMBERS
 ******************************************************************************/

/*
 * IRQ numbers defined by Cortex-M vector table.
 */

#define USART1_IRQ_NUMBER 37u
#define USART2_IRQ_NUMBER 38u
#define USART6_IRQ_NUMBER 71u


/******************************************************************************
 *                      RCC CLOCK ENABLE BIT DEFINITIONS
 ******************************************************************************/

#define RCC_AHB1ENR_GPIOAEN   (1UL << 0)
#define RCC_APB1ENR_USART2EN  (1UL << 17)
#define RCC_APB2ENR_USART1EN  (1UL << 4)
#define RCC_APB2ENR_USART6EN  (1UL << 5)


/******************************************************************************
 *                      GPIO REGISTER STRUCTURE
 ******************************************************************************/

/*
 * Represents GPIO register layout.
 * Structure order MUST match reference manual.
 */

typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFR[2];
} GPIO_TypeDef;


/******************************************************************************
 *                      RCC REGISTER STRUCTURE
 ******************************************************************************/

/*
 * Reset and Clock Control register mapping.
 * Only required registers are accessed by this module.
 */

typedef struct {
    volatile uint32_t CR;
    volatile uint32_t PLLCFGR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t AHB1RSTR;
    volatile uint32_t AHB2RSTR;
    volatile uint32_t RESERVED0[2];
    volatile uint32_t APB1RSTR;
    volatile uint32_t APB2RSTR;
    volatile uint32_t RESERVED1[2];
    volatile uint32_t AHB1ENR;
    volatile uint32_t AHB2ENR;
    volatile uint32_t RESERVED2[2];
    volatile uint32_t APB1ENR;
    volatile uint32_t APB2ENR;
} RCC_TypeDef;


/* Peripheral instances */
#define GPIOA ((GPIO_TypeDef *)GPIOA_BASE)
#define RCC   ((RCC_TypeDef *)RCC_BASE)


/******************************************************************************
 *                          CLOCK CONFIGURATION
 ******************************************************************************/

/*
 * Default clock definitions.
 * Can be overridden by build system if needed.
 */

#ifndef SYSTEM_CORE_CLOCK_HZ
#define SYSTEM_CORE_CLOCK_HZ 16000000UL
#endif

#ifndef APB1_PERIPHERAL_CLOCK_HZ
#define APB1_PERIPHERAL_CLOCK_HZ SYSTEM_CORE_CLOCK_HZ
#endif

#ifndef APB2_PERIPHERAL_CLOCK_HZ
#define APB2_PERIPHERAL_CLOCK_HZ SYSTEM_CORE_CLOCK_HZ
#endif


/******************************************************************************
 *                      GLOBAL UART OBJECTS
 ******************************************************************************/

/*
 * These objects represent logical serial interfaces
 * exposed to application code.
 */

Uart Serial;
Uart Serial1;
Uart Serial6;


/******************************************************************************
 *                      NVIC INTERRUPT ENABLE FUNCTION
 ******************************************************************************/

/*
 * Enables interrupt inside NVIC.
 *
 * irq_number:
 *      position inside vector table
 */

static void nvic_enable_irq(uint32_t irq_number)
{
    if (irq_number < 32u) {
        NVIC_ISER0 = (1UL << irq_number);
    } else {
        NVIC_ISER1 = (1UL << (irq_number - 32u));
    }
}


/******************************************************************************
 *                      GPIO ALTERNATE FUNCTION CONFIGURATION
 ******************************************************************************/

/*
 * Configures pin as Alternate Function mode.
 *
 * Steps performed:
 *  1. Select Alternate Mode
 *  2. Push-pull output
 *  3. Medium speed
 *  4. Pull-up enabled
 *  5. Select AF number
 */

static void gpio_config_alternate(GPIO_TypeDef *port,
                                  uint8_t pin,
                                  uint8_t alternate_function)
{
    uint32_t position = (uint32_t)pin * 2u;
    uint32_t afr_index = pin / 8u;
    uint32_t afr_position = (uint32_t)(pin % 8u) * 4u;

    port->MODER &= ~(3UL << position);
    port->MODER |=  (2UL << position);

    port->OTYPER &= ~(1UL << pin);

    port->OSPEEDR &= ~(3UL << position);
    port->OSPEEDR |=  (2UL << position);

    port->PUPDR &= ~(3UL << position);
    port->PUPDR |=  (1UL << position);

    port->AFR[afr_index] &= ~(0xFUL << afr_position);
    port->AFR[afr_index] |= ((uint32_t)alternate_function << afr_position);
}


/******************************************************************************
 *                      USART CLOCK SELECTION
 ******************************************************************************/

/*
 * Returns peripheral clock used by selected UART.
 */

static uint32_t get_usart_clock(const Uart *uart)
{
    if (uart->instance == STM32F411RE_USART2) {
        return APB1_PERIPHERAL_CLOCK_HZ;
    }

    return APB2_PERIPHERAL_CLOCK_HZ;
}


/******************************************************************************
 *                      USART1 BOARD INITIALIZATION
 ******************************************************************************/

static void board_usart1_init(Uart *uart)
{
    (void)uart;

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    (void)RCC->AHB1ENR;
    (void)RCC->APB2ENR;

    gpio_config_alternate(GPIOA, 9u, 7u);
    gpio_config_alternate(GPIOA, 10u, 7u);

    nvic_enable_irq(USART1_IRQ_NUMBER);
}


/******************************************************************************
 *                      USART2 BOARD INITIALIZATION
 ******************************************************************************/

static void board_usart2_init(Uart *uart)
{
    (void)uart;

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    (void)RCC->AHB1ENR;
    (void)RCC->APB1ENR;

    gpio_config_alternate(GPIOA, 2u, 7u);
    gpio_config_alternate(GPIOA, 3u, 7u);

    nvic_enable_irq(USART2_IRQ_NUMBER);
}


/******************************************************************************
 *                      USART6 BOARD INITIALIZATION
 ******************************************************************************/

static void board_usart6_init(Uart *uart)
{
    (void)uart;

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART6EN;

    (void)RCC->AHB1ENR;
    (void)RCC->APB2ENR;

    gpio_config_alternate(GPIOA, 11u, 8u);
    gpio_config_alternate(GPIOA, 12u, 8u);

    nvic_enable_irq(USART6_IRQ_NUMBER);
}


/******************************************************************************
 *                      USART DEINITIALIZATION
 ******************************************************************************/

static void board_usart_deinit(Uart *uart)
{
    if (uart->instance == STM32F411RE_USART2) {
        RCC->APB1ENR &= ~RCC_APB1ENR_USART2EN;
    }
    else if (uart->instance == STM32F411RE_USART1) {
        RCC->APB2ENR &= ~RCC_APB2ENR_USART1EN;
    }
    else if (uart->instance == STM32F411RE_USART6) {
        RCC->APB2ENR &= ~RCC_APB2ENR_USART6EN;
    }
}


/******************************************************************************
 *                      BOARD UART INITIALIZATION
 ******************************************************************************/

void board_uart_init_all(void)
{
    uart_init_object(&Serial,
                     STM32F411RE_USART2,
                     board_usart2_init,
                     board_usart_deinit,
                     get_usart_clock);

    uart_init_object(&Serial1,
                     STM32F411RE_USART1,
                     board_usart1_init,
                     board_usart_deinit,
                     get_usart_clock);

    uart_init_object(&Serial6,
                     STM32F411RE_USART6,
                     board_usart6_init,
                     board_usart_deinit,
                     get_usart_clock);
}


/******************************************************************************
 *                      INTERRUPT SERVICE ROUTINES
 ******************************************************************************/

/*
 * ISR must remain minimal.
 * Only forward interrupt to driver layer.
 */

void USART1_IRQHandler(void)
{
    uart_irq_handler(&Serial1);
}

void USART2_IRQHandler(void)
{
    uart_irq_handler(&Serial);
}

void USART6_IRQHandler(void)
{
    uart_irq_handler(&Serial6);
}
