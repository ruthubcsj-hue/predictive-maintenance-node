/* Vendor-neutral contract for status indicators and the board millisecond clock. */
#ifndef BOARD_IO_H
#define BOARD_IO_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initializes both predictive-maintenance status indicators.
 *
 * The platform configures the green and red indicators and leaves both off.
 *
 * @pre The system clock configuration has completed.
 * @post Both indicator outputs are ready and inactive.
 */
void board_io_init(void);

/**
 * @brief Applies the logical green and red status-indicator states.
 *
 * @param[in] green_is_on true turns the normal-state indicator on.
 * @param[in] red_is_on true turns the anomaly-state indicator on.
 *
 * @pre board_io_init() has completed.
 * @post Both indicator pins reflect the requested states.
 */
void board_io_set_status_leds(bool green_is_on, bool red_is_on);

/**
 * @brief Returns monotonic board time in milliseconds.
 *
 * The counter wraps naturally; use unsigned subtraction for elapsed-time checks.
 *
 * @return Milliseconds since board_io_init() initialized the platform timebase.
 */
uint32_t board_io_millis(void);

/**
 * @brief Waits for the next interrupt to reduce foreground polling activity.
 *
 * @note The call returns when an enabled interrupt wakes the processor.
 */
void board_io_wait_for_interrupt(void);

#endif /* BOARD_IO_H */
