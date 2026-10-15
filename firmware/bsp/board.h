/* Declarative STM32F4DISCOVERY wiring and active-level facts. */
#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"

#ifndef BOARD_NAME
#define BOARD_NAME "STM32F4DISCOVERY"
#endif

/**
 * @brief Returns the board identity string used by diagnostics.
 *
 * @return Constant, null-terminated board name.
 */
const char *board_name(void);

#define BOARD_GREEN_LED_ACTIVE_LEVEL 1U
#define BOARD_RED_LED_ACTIVE_LEVEL  1U

/**
 * @brief Converts the logical green-indicator state to its active-high level.
 *
 * @param[in] is_on true requests LD4 on; false requests it off.
 * @return Electrical output level for the active-high LD4 connection.
 */
static inline uint32_t board_green_led_level(bool is_on)
{
    return is_on ? BOARD_GREEN_LED_ACTIVE_LEVEL : (uint32_t)!BOARD_GREEN_LED_ACTIVE_LEVEL;
}

/**
 * @brief Converts the logical red-indicator state to its active-high level.
 *
 * @param[in] is_on true requests LD5 on; false requests it off.
 * @return Electrical output level for the active-high LD5 connection.
 */
static inline uint32_t board_red_led_level(bool is_on)
{
    return is_on ? BOARD_RED_LED_ACTIVE_LEVEL : (uint32_t)!BOARD_RED_LED_ACTIVE_LEVEL;
}

#endif /* BOARD_H */
