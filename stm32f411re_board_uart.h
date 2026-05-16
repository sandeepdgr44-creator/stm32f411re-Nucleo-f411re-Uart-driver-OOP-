/******************************************************************************
 * @file    stm32f411re_board_uart.h
 * @author  Sandeep Ray
 *
 * @brief
 * Board level UART abstraction layer for STM32F411RE.
 *
 * This header connects the generic UART driver with the
 * physical peripherals available on the STM32F411RE board.
 *
 * The purpose of this file is:
 *  - Map hardware base addresses
 *  - Expose board UART instances
 *  - Provide interrupt prototypes
 *  - Initialize all UART peripherals used by the board
 *
 * This layer separates:
 *
 *      Application
 *          ↓
 *      Board Layer (THIS FILE)
 *          ↓
 *      UART Driver
 *          ↓
 *      STM32 Hardware
 *
 * Keeping this separation allows:
 *  - driver reuse across multiple boards
 *  - clean portability
 *  - easier maintenance
 *
 ******************************************************************************/

#ifndef STM32F411RE_BOARD_UART_H
#define STM32F411RE_BOARD_UART_H

#include "stm32f411re_uart.h"

/*
 * C++ Compatibility
 *
 * When this header is included inside a C++ project,
 * name mangling must be disabled for interrupt handlers
 * and exported symbols.
 *
 * extern "C" ensures linker compatibility between
 * C and C++ compilation units.
 */
#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************
 *                          USART BASE ADDRESSES
 ******************************************************************************/

/*
 * STM32 peripherals are memory mapped.
 *
 * Each USART peripheral resides at a fixed address
 * defined by the STM32F411 reference manual.
 *
 * These values MUST match the official memory map.
 *
 * Only unsigned long constants are used to avoid
 * implicit type conversion issues (MISRA compliance).
 */

/* USART1 base address (APB2 Bus) */
#define STM32F411RE_USART1_BASE   (0x40011000UL)

/* USART2 base address (APB1 Bus) */
#define STM32F411RE_USART2_BASE   (0x40004400UL)

/* USART6 base address (APB2 Bus) */
#define STM32F411RE_USART6_BASE   (0x40011400UL)


/******************************************************************************
 *                      PERIPHERAL POINTER DEFINITIONS
 ******************************************************************************/

/*
 * These macros convert raw base addresses into
 * structured peripheral register access.
 *
 * USART_TypeDef is defined in stm32f411re_uart.h
 * and represents the USART register layout.
 *
 * Example:
 *      STM32F411RE_USART1->DR
 * accesses the USART1 Data Register directly.
 */

#define STM32F411RE_USART1   ((USART_TypeDef *)STM32F411RE_USART1_BASE)
#define STM32F411RE_USART2   ((USART_TypeDef *)STM32F411RE_USART2_BASE)
#define STM32F411RE_USART6   ((USART_TypeDef *)STM32F411RE_USART6_BASE)


/******************************************************************************
 *                      BOARD UART OBJECT DECLARATIONS
 ******************************************************************************/

/*
 * These UART objects represent logical serial ports
 * exposed to the application layer.
 *
 * Naming convention follows Arduino-style familiarity:
 *
 *      Serial   → USART2 (typically debugging UART)
 *      Serial1  → USART1
 *      Serial6  → USART6
 *
 * Actual mapping is performed in the board source file.
 *
 * The objects are declared here as extern so multiple
 * modules can access the same UART instances without
 * duplicating memory.
 */

extern Uart Serial;
extern Uart Serial1;
extern Uart Serial6;


/******************************************************************************
 *                      BOARD INITIALIZATION FUNCTION
 ******************************************************************************/

/*
 * Initializes every UART used by the board.
 *
 * Responsibilities typically include:
 *  - enabling peripheral clocks
 *  - configuring GPIO alternate functions
 *  - setting baud rate
 *  - enabling RX/TX
 *  - enabling interrupts
 *
 * This function should be called once during system startup.
 */
void board_uart_init_all(void);


/******************************************************************************
 *                      INTERRUPT HANDLER DECLARATIONS
 ******************************************************************************/

/*
 * These are Cortex-M interrupt service routines.
 *
 * They must EXACTLY match the names defined inside
 * the STM32 startup vector table.
 *
 * Each handler forwards hardware interrupts to the
 * generic UART driver.
 *
 * NOTE:
 * Do NOT rename these functions.
 * Linker will fail if names mismatch.
 */

/* USART1 interrupt handler */
void USART1_IRQHandler(void);

/* USART2 interrupt handler */
void USART2_IRQHandler(void);

/* USART6 interrupt handler */
void USART6_IRQHandler(void);


#ifdef __cplusplus
}
#endif

#endif /* STM32F411RE_BOARD_UART_H */
