/* Application and onboard peripheral configuration for predictive maintenance. */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define APP_LED_GPIO_PORT                         GPIOD
#define APP_LED_GPIO_PIN                          LL_GPIO_PIN_12
#define APP_RED_LED_GPIO_PORT                     GPIOD
#define APP_RED_LED_GPIO_PIN                      LL_GPIO_PIN_14

#define APP_ORANGE_LED_GPIO_PORT                  GPIOD
#define APP_ORANGE_LED_GPIO_PIN                   LL_GPIO_PIN_13
#define APP_BLUE_LED_GPIO_PORT                    GPIOD
#define APP_BLUE_LED_GPIO_PIN                     LL_GPIO_PIN_15
#define APP_BUTTON_GPIO_PORT                      GPIOA
#define APP_BUTTON_GPIO_PIN                       LL_GPIO_PIN_0
#define APP_BUTTON_DEBOUNCE_MS                    200U
/* Optional external active buzzer: + to PA1, - to GND. Harmless if not fitted. */
#define APP_BUZZER_GPIO_PORT                      GPIOA
#define APP_BUZZER_GPIO_PIN                       LL_GPIO_PIN_1
#define APP_BUZZER_BEEP_PERIOD_MS                 200U
#define APP_VIBRATION_WARNING_MULTIPLIER          1.5F

#define APP_UART_TX_GPIO_PORT                     GPIOA
#define APP_UART_TX_GPIO_PIN                      LL_GPIO_PIN_2
#define APP_UART_BAUD_RATE                        115200U
#define APP_UART_APB1_PRESCALER                   4U
#define APP_UART_TX_TIMEOUT_MS                    2U

#define APP_ACCELEROMETER_SCK_GPIO_PORT           GPIOA
#define APP_ACCELEROMETER_SCK_GPIO_PIN            LL_GPIO_PIN_5
#define APP_ACCELEROMETER_MISO_GPIO_PORT          GPIOA
#define APP_ACCELEROMETER_MISO_GPIO_PIN           LL_GPIO_PIN_6
#define APP_ACCELEROMETER_MOSI_GPIO_PORT          GPIOA
#define APP_ACCELEROMETER_MOSI_GPIO_PIN           LL_GPIO_PIN_7
#define APP_ACCELEROMETER_CS_GPIO_PORT            GPIOE
#define APP_ACCELEROMETER_CS_GPIO_PIN             LL_GPIO_PIN_3

#define APP_VIBRATION_SAMPLE_PERIOD_MS            10U
#define APP_VIBRATION_SAMPLES_PER_WINDOW           100U
#define APP_BASELINE_WINDOW_COUNT                 10U
#define APP_ANOMALY_CLEAR_WINDOW_COUNT            3U
#define APP_TEMPERATURE_ANOMALY_WINDOW_COUNT       3U
#define APP_STATUS_REPORT_PERIOD_MS               1000U
#define APP_SENSOR_SPI_TIMEOUT_MS                 2U
#define APP_ADC_CONVERSION_TIMEOUT_MS             5U
#define APP_SENSOR_STARTUP_SETTLE_MS              1U

#define APP_VIBRATION_ANOMALY_MULTIPLIER          2.0F
#define APP_ACCELEROMETER_LSB_G                   0.00012F
#define APP_ACCELEROMETER_SENSITIVITY_MG_PER_LSB  0.12F
#define APP_ACCELEROMETER_OUTPUT_SHIFT            0U
#define APP_ACCELEROMETER_FULL_SCALE_REGISTER     0x08U
#define APP_ACCELEROMETER_FILTER_REGISTER         0x00U
#define APP_ACCELEROMETER_DATA_RATE_REGISTER      0x60U
#define APP_ACCELEROMETER_ENABLE_AXES_REGISTER    0x07U
#define APP_ACCELEROMETER_ID_REGISTER             0x0FU
#define APP_ACCELEROMETER_ID_VALUE                0x3FU
#define APP_ACCELEROMETER_CTRL4_REGISTER          0x20U
#define APP_ACCELEROMETER_CTRL5_REGISTER          0x24U
#define APP_ACCELEROMETER_OUTPUT_REGISTER         0x28U
#define APP_ACCELEROMETER_READ_COMMAND            0x80U
/* LIS3DSH has no multi-byte command bit: bit 6 is an address bit. Burst reads use
 * register auto-increment (CTRL_REG6 IF_ADD_INC), so this stays 0. */
#define APP_ACCELEROMETER_MULTI_BYTE_COMMAND      0x00U
#define APP_ACCELEROMETER_BDU_BIT                 0x08U
#define APP_ACCELEROMETER_CTRL6_REGISTER          0x25U
#define APP_ACCELEROMETER_CTRL6_VALUE             0x10U
#define APP_ADC_VDDA_MIN_MV                       1800U
#define APP_ADC_VDDA_MAX_MV                       3600U
#define APP_ADC_TEMPERATURE_MIN_C                 (-40)
#define APP_ADC_TEMPERATURE_MAX_C                 150
#define APP_TEMPERATURE_ANOMALY_THRESHOLD_C       70.0F

#endif /* APP_CONFIG_H */
