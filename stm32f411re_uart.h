/*
===============================================================================
File Name   : stm32f411re_uart.h
Author      : Sandeep Ray
Description : Generic UART driver interface for STM32F411RE

This file defines a reusable, hardware-independent UART abstraction layer.
The objective of this design is:

    - Clean separation between board layer and driver layer
    - Reusable UART API similar to Arduino Serial concept
    - Interrupt driven non-blocking communication
    - MISRA-C friendly structure design
    - Portable across multiple STM32 boards

The driver exposes a high level API while allowing board specific
initialization through callback hooks.

===============================================================================
*/

#ifndef STM32F411RE_UART_H
#define STM32F411RE_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
                        CONFIGURABLE BUFFER SIZE
=============================================================================*/

/*
 * TX circular buffer size.
 * Must be power of two for optimal performance.
 */
#ifndef UART_TX_BUFFER_SIZE
#define UART_TX_BUFFER_SIZE 128u
#endif

/*
 * RX circular buffer size.
 */
#ifndef UART_RX_BUFFER_SIZE
#define UART_RX_BUFFER_SIZE 128u
#endif


/*=============================================================================
                    USART REGISTER HARDWARE MAPPING
=============================================================================*/

/*
 * Minimal USART register map.
 * Only registers required by driver are defined.
 *
 * volatile keyword ensures:
 *  - compiler never optimizes accesses
 *  - every read/write hits hardware register
 */
typedef struct {
    volatile uint32_t SR;     /* Status register               */
    volatile uint32_t DR;     /* Data register                 */
    volatile uint32_t BRR;    /* Baudrate register             */
    volatile uint32_t CR1;    /* Control register 1            */
    volatile uint32_t CR2;    /* Control register 2            */
    volatile uint32_t CR3;    /* Control register 3            */
    volatile uint32_t GTPR;   /* Guard time / prescaler        */
} USART_TypeDef;


/*=============================================================================
                        UART CONFIGURATION ENUMS
=============================================================================*/

/* Word length configuration */
typedef enum {
    UART_WORD_LENGTH_8B = 0u,
    UART_WORD_LENGTH_9B = 1u
} UartWordLength;

/* Parity configuration */
typedef enum {
    UART_PARITY_NONE = 0u,
    UART_PARITY_EVEN,
    UART_PARITY_ODD
} UartParity;

/* Stop bit configuration */
typedef enum {
    UART_STOP_BITS_1 = 0u,
    UART_STOP_BITS_0_5 = 1u,
    UART_STOP_BITS_2 = 2u,
    UART_STOP_BITS_1_5 = 3u
} UartStopBits;

/* Oversampling configuration */
typedef enum {
    UART_OVERSAMPLING_16 = 0u,
    UART_OVERSAMPLING_8  = 1u
} UartOversampling;


/*=============================================================================
                        UART RUNTIME CONFIG STRUCTURE
=============================================================================*/

/*
 * Complete runtime configuration used during uart_begin().
 */
typedef struct {

    uint32_t baudrate;              /* Desired baudrate                 */
    uint32_t peripheral_clock_hz;   /* Clock feeding USART peripheral   */

    UartWordLength word_length;
    UartParity parity;
    UartStopBits stop_bits;
    UartOversampling oversampling;

} UartConfig;


/* Forward declaration */
typedef struct Uart Uart;


/*=============================================================================
                    BOARD ABSTRACTION CALLBACK TYPES
=============================================================================*/

/*
 * Board initialization function.
 * Responsible for:
 *      - GPIO configuration
 *      - Clock enable
 *      - NVIC setup
 */
typedef void (*UartBoardInitFn)(Uart *uart);

/*
 * Board deinitialization.
 */
typedef void (*UartBoardDeInitFn)(Uart *uart);

/*
 * Returns actual peripheral clock frequency.
 */
typedef uint32_t (*UartClockFn)(const Uart *uart);


/*=============================================================================
                        HIGH LEVEL UART API TABLE
=============================================================================*/

/*
 * Function pointer table.
 *
 * Allows:
 *      - Object oriented style in C
 *      - Multiple UART instances sharing same API
 *      - Runtime polymorphism
 */
typedef struct {

    void   (*begin)(Uart *uart, uint32_t baudrate);
    void   (*end)(Uart *uart);

    int    (*available)(Uart *uart);
    int    (*available_for_write)(Uart *uart);

    int    (*peek)(Uart *uart);
    int    (*read)(Uart *uart);

    void   (*flush)(Uart *uart);

    size_t (*write)(Uart *uart, const uint8_t *data, size_t length);
    size_t (*write_byte)(Uart *uart, uint8_t data);
    size_t (*write_async)(Uart *uart, const uint8_t *data, size_t length);

    size_t (*print)(Uart *uart, const char *text);
    size_t (*println)(Uart *uart, const char *text);

    size_t (*print_int)(Uart *uart, int32_t value, uint8_t base);
    size_t (*print_uint)(Uart *uart, uint32_t value, uint8_t base);
    size_t (*print_float)(Uart *uart, float value, uint8_t digits);

} UartApi;


/*=============================================================================
                            UART OBJECT
=============================================================================*/

/*
 * Main UART instance structure.
 *
 * Design philosophy:
 *  - Entire driver state stored inside object
 *  - Multiple UARTs supported simultaneously
 *  - No global hidden state
 */
struct Uart {

    USART_TypeDef *instance;     /* Hardware peripheral base address */

    UartConfig config;           /* Active configuration */

    UartBoardInitFn board_init;
    UartBoardDeInitFn board_deinit;
    UartClockFn get_clock_hz;

    const UartApi *api;          /* API function table */

    /*================ TX BUFFER =================*/
    volatile uint8_t  tx_buffer[UART_TX_BUFFER_SIZE];
    volatile uint16_t tx_head;
    volatile uint16_t tx_tail;

    /*================ RX BUFFER =================*/
    volatile uint8_t  rx_buffer[UART_RX_BUFFER_SIZE];
    volatile uint16_t rx_head;
    volatile uint16_t rx_tail;
};


/*=============================================================================
                        DRIVER INITIALIZATION
=============================================================================*/

void uart_init_object(Uart *uart,
                      USART_TypeDef *instance,
                      UartBoardInitFn board_init,
                      UartBoardDeInitFn board_deinit,
                      UartClockFn get_clock_hz);


/*=============================================================================
                        BASIC CONTROL FUNCTIONS
=============================================================================*/

void uart_begin(Uart *uart, uint32_t baudrate);
void uart_begin_config(Uart *uart, const UartConfig *config);
void uart_end(Uart *uart);

void uart_set_baudrate(Uart *uart, uint32_t baudrate);

/*
 * Computes BRR register value based on clock and oversampling.
 */
uint32_t uart_make_brr(uint32_t peripheral_clock_hz,
                       uint32_t baudrate,
                       UartOversampling oversampling);


/*=============================================================================
                        BUFFER MANAGEMENT
=============================================================================*/

int uart_available(Uart *uart);
int uart_available_for_write(Uart *uart);

int uart_peek(Uart *uart);
int uart_read(Uart *uart);

void uart_flush(Uart *uart);


/*=============================================================================
                        TRANSMISSION FUNCTIONS
=============================================================================*/

size_t uart_write(Uart *uart, const uint8_t *data, size_t length);
size_t uart_write_byte(Uart *uart, uint8_t data);
size_t uart_write_async(Uart *uart, const uint8_t *data, size_t length);

size_t uart_print(Uart *uart, const char *text);
size_t uart_println(Uart *uart, const char *text);

size_t uart_print_int(Uart *uart, int32_t value, uint8_t base);
size_t uart_print_uint(Uart *uart, uint32_t value, uint8_t base);
size_t uart_print_float(Uart *uart, float value, uint8_t digits);


/*=============================================================================
                        INTERRUPT HANDLER
=============================================================================*/

/*
 * Called from actual IRQ vector.
 * Handles RXNE and TXE events.
 */
void uart_irq_handler(Uart *uart);


/*=============================================================================
                        USER FRIENDLY MACROS
=============================================================================*/

/*
 * Arduino style wrapper macros.
 * Keeps syntax clean in application layer.
 */

#define Serial_begin(serial_, baud_)              ((serial_).api->begin(&(serial_), (baud_)))
#define Serial_end(serial_)                       ((serial_).api->end(&(serial_)))

#define Serial_available(serial_)                 ((serial_).api->available(&(serial_)))
#define Serial_availableForWrite(serial_)         ((serial_).api->available_for_write(&(serial_)))

#define Serial_peek(serial_)                      ((serial_).api->peek(&(serial_)))
#define Serial_read(serial_)                      ((serial_).api->read(&(serial_)))

#define Serial_flush(serial_)                     ((serial_).api->flush(&(serial_)))

#define Serial_write(serial_, data_, len_)        ((serial_).api->write(&(serial_), (const uint8_t *)(data_), (len_)))
#define Serial_writeByte(serial_, data_)          ((serial_).api->write_byte(&(serial_), (uint8_t)(data_)))
#define Serial_writeAsync(serial_, data_, len_)   ((serial_).api->write_async(&(serial_), (const uint8_t *)(data_), (len_)))

#define Serial_print(serial_, text_)              ((serial_).api->print(&(serial_), (text_)))
#define Serial_println(serial_, text_)            ((serial_).api->println(&(serial_), (text_)))

#define Serial_printInt(serial_, value_, base_)   ((serial_).api->print_int(&(serial_), (value_), (base_)))
#define Serial_printUInt(serial_, value_, base_)  ((serial_).api->print_uint(&(serial_), (value_), (base_)))
#define Serial_printFloat(serial_, value_, digs_) ((serial_).api->print_float(&(serial_), (value_), (digs_)))


#ifdef __cplusplus
}
#endif

#endif
