/* STM32F4 USART2 TX console and newlib stdout/stderr transport. */
#include "console.h"

#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>

#include "app_config.h"
#include "board_io.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_usart.h"

/* Waits at most APP_UART_TX_TIMEOUT_MS for TXE, then writes one byte. */
static bool console_stm32_write_byte(uint8_t byte_value)
{
    uint32_t started_ms = board_io_millis();
    bool is_ready = false;

    while (is_ready == false)
    {
        if (LL_USART_IsActiveFlag_TXE(USART2) != 0U)
        {
            is_ready = true;
        }
        else if ((uint32_t)(board_io_millis() - started_ms) >= APP_UART_TX_TIMEOUT_MS)
        {
            return false;
        }
    }

    LL_USART_TransmitData8(USART2, byte_value);
    return true;
}

void console_init(void)
{
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);

    LL_GPIO_SetPinMode(APP_UART_TX_GPIO_PORT, APP_UART_TX_GPIO_PIN,
                       LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinOutputType(APP_UART_TX_GPIO_PORT, APP_UART_TX_GPIO_PIN,
                             LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(APP_UART_TX_GPIO_PORT, APP_UART_TX_GPIO_PIN,
                        LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinPull(APP_UART_TX_GPIO_PORT, APP_UART_TX_GPIO_PIN,
                       LL_GPIO_PULL_NO);
    LL_GPIO_SetAFPin_0_7(APP_UART_TX_GPIO_PORT, APP_UART_TX_GPIO_PIN,
                         LL_GPIO_AF_7);

    LL_USART_Disable(USART2);
    LL_USART_SetBaudRate(USART2, SystemCoreClock / APP_UART_APB1_PRESCALER,
                         LL_USART_OVERSAMPLING_16, APP_UART_BAUD_RATE);
    LL_USART_SetDataWidth(USART2, LL_USART_DATAWIDTH_8B);
    LL_USART_SetStopBitsLength(USART2, LL_USART_STOPBITS_1);
    LL_USART_SetParity(USART2, LL_USART_PARITY_NONE);
    LL_USART_SetTransferDirection(USART2, LL_USART_DIRECTION_TX);
    LL_USART_SetHWFlowCtrl(USART2, LL_USART_HWCONTROL_NONE);
    LL_USART_SetOverSampling(USART2, LL_USART_OVERSAMPLING_16);
    LL_USART_Enable(USART2);
}

/**
 * @brief Sends newlib stdout/stderr bytes through USART2 TX.
 *
 * Newlines are expanded to CRLF. The operation uses a bounded transmit-ready
 * wait and can return a short count if USART2 does not become ready.
 *
 * @param[in] file_descriptor Newlib descriptor; only 1 (stdout) and 2 (stderr) are supported.
 * @param[in] buffer Borrowed byte buffer to transmit; it is not retained.
 * @param[in] length Number of bytes in buffer; must be non-negative.
 *
 * @return Number of bytes accepted, or -1 if the descriptor or buffer is invalid
 *         or a USART transmit wait times out.
 */
int _write(int file_descriptor, char *buffer, int length)
{
    int byte_index;

    if ((buffer == NULL) || (length < 0) ||
        ((file_descriptor != 1) && (file_descriptor != 2)))
    {
        errno = EINVAL;
        return -1;
    }

    for (byte_index = 0; byte_index < length; byte_index++)
    {
        uint8_t current_byte = (uint8_t)buffer[byte_index];

        if ((current_byte == (uint8_t)'\n') &&
            !console_stm32_write_byte((uint8_t)'\r'))
        {
            errno = EIO;
            return -1;
        }

        if (!console_stm32_write_byte(current_byte))
        {
            errno = EIO;
            return -1;
        }
    }

    return length;
}

/**
 * @brief Identifies standard output streams as terminals for newlib buffering.
 *
 * @param[in] file_descriptor Newlib file descriptor to check.
 *
 * @return 1 for stdout/stderr, or -1 with errno set for unsupported descriptors.
 */
int _isatty(int file_descriptor)
{
    if ((file_descriptor == 1) || (file_descriptor == 2))
    {
        return 1;
    }

    errno = EBADF;
    return -1;
}
