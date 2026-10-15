/* STM32F4 SysTick bridge used by the Cube interrupt handler. */
#ifndef BOARD_IO_STM32_H
#define BOARD_IO_STM32_H

/**
 * @brief Advances the STM32 platform's millisecond timebase from SysTick.
 *
 * @note Call exactly once per 1 ms SysTick interrupt.
 */
void board_io_stm32_systick_isr(void);

#endif /* BOARD_IO_STM32_H */
