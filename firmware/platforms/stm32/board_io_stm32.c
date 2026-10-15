/* STM32F4DISCOVERY adapter for active-high LD4/LD5 and SysTick time. */
#include "board_io.h"
#include "board_io_stm32.h"

#include <stdint.h>

#include "app_config.h"
#include "board.h"
#include "stm32f4xx.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_gpio.h"

static volatile uint32_t platform_stm32_millisecond_count;

void board_io_init(void)
{
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOD);

    LL_GPIO_ResetOutputPin(APP_LED_GPIO_PORT, APP_LED_GPIO_PIN);
    LL_GPIO_ResetOutputPin(APP_RED_LED_GPIO_PORT, APP_RED_LED_GPIO_PIN);

    LL_GPIO_SetPinMode(APP_LED_GPIO_PORT, APP_LED_GPIO_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(APP_LED_GPIO_PORT, APP_LED_GPIO_PIN,
                             LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(APP_LED_GPIO_PORT, APP_LED_GPIO_PIN,
                        LL_GPIO_SPEED_FREQ_LOW);
    LL_GPIO_SetPinPull(APP_LED_GPIO_PORT, APP_LED_GPIO_PIN, LL_GPIO_PULL_NO);

    LL_GPIO_SetPinMode(APP_RED_LED_GPIO_PORT, APP_RED_LED_GPIO_PIN,
                       LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(APP_RED_LED_GPIO_PORT, APP_RED_LED_GPIO_PIN,
                             LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(APP_RED_LED_GPIO_PORT, APP_RED_LED_GPIO_PIN,
                        LL_GPIO_SPEED_FREQ_LOW);
    LL_GPIO_SetPinPull(APP_RED_LED_GPIO_PORT, APP_RED_LED_GPIO_PIN,
                       LL_GPIO_PULL_NO);

    platform_stm32_millisecond_count = 0U;
}

void board_io_set_status_leds(bool green_is_on, bool red_is_on)
{
    if (board_green_led_level(green_is_on) != 0U)
    {
        LL_GPIO_SetOutputPin(APP_LED_GPIO_PORT, APP_LED_GPIO_PIN);
    }
    else
    {
        LL_GPIO_ResetOutputPin(APP_LED_GPIO_PORT, APP_LED_GPIO_PIN);
    }

    if (board_red_led_level(red_is_on) != 0U)
    {
        LL_GPIO_SetOutputPin(APP_RED_LED_GPIO_PORT, APP_RED_LED_GPIO_PIN);
    }
    else
    {
        LL_GPIO_ResetOutputPin(APP_RED_LED_GPIO_PORT, APP_RED_LED_GPIO_PIN);
    }
}

uint32_t board_io_millis(void)
{
    return platform_stm32_millisecond_count;
}

void board_io_wait_for_interrupt(void)
{
    __WFI();
}

void board_io_stm32_systick_isr(void)
{
    platform_stm32_millisecond_count++;
}
