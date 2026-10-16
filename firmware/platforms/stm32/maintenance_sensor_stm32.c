/* STM32F4DISCOVERY SPI1 LIS3DSH and internal ADC-temperature adapter. */
#include "maintenance_sensor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_config.h"
#include "board_io.h"
#include "stm32f4xx.h"
#include "stm32f4xx_ll_adc.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_spi.h"

#define SENSOR_SPI_MAX_TRANSFER_BYTES (6U)
#define SENSOR_SPI_TIMEOUT_MS         (APP_SENSOR_SPI_TIMEOUT_MS)
#define SENSOR_ADC_TIMEOUT_MS         (APP_ADC_CONVERSION_TIMEOUT_MS)

static bool sensor_spi_is_initialized;
static bool sensor_adc_is_initialized;
static bool sensor_is_ready;
static bool sensor_io_fault;

static bool sensor_spi_init(void);
static bool sensor_spi_exchange(uint8_t tx_byte, uint8_t *rx_byte);
static bool sensor_spi_wait_idle(void);
static bool sensor_spi_read(uint8_t address, uint8_t *data, uint16_t size);
static bool sensor_spi_write(uint8_t address, const uint8_t *data, uint16_t size);
static bool sensor_lis3dsh_init(void);
static bool sensor_adc_init(void);
static bool sensor_adc_convert(uint32_t channel, uint16_t *value);
static void sensor_adc_stabilization_delay(void);

/**
 * @brief Initializes LIS3DSH over the board's SPI1 wiring and configures ADC1.
 *
 * SPI1 PA5/PA6/PA7 and PE3 CS are grounded by the board record. SPI mode 0,
 * 100 Hz output, four-wire operation and the ±4 g scale follow the CubeF4
 * Discovery BSP and the accepted defaults. ADC1 samples the internal die
 * temperature sensor and VREFINT, using factory calibration and measured VDDA.
 *
 * @return true when both acquisition paths initialize; otherwise false.
 */
bool maintenance_sensor_init(void)
{
    sensor_is_ready = false;
    sensor_io_fault = false;

    if (!sensor_lis3dsh_init() || !sensor_adc_init())
    {
        return false;
    }

    sensor_is_ready = true;
    return true;
}

/**
 * @brief Reads the onboard LIS3DSH X/Y/Z acceleration in g.
 *
 * Raw data follows the LIS3DSH 16-bit two's-complement output format and is
 * converted using the ±4 g sensitivity of 0.12 mg/LSB.
 *
 * @param[out] acceleration_g Three-element X/Y/Z destination in g.
 * @return true when a complete valid sample was read; false on invalid input,
 *         missing initialization, or any bounded SPI timeout.
 * @pre maintenance_sensor_init() returned true.
 */
bool maintenance_sensor_read_acceleration(float acceleration_g[3])
{
    uint8_t raw[6];
    uint8_t axis_index;

    if ((acceleration_g == NULL) || !sensor_is_ready)
    {
        return false;
    }

    sensor_io_fault = false;
    if (!sensor_spi_read(APP_ACCELEROMETER_OUTPUT_REGISTER, raw, sizeof(raw)) ||
        sensor_io_fault)
    {
        return false;
    }

    for (axis_index = 0U; axis_index < 3U; axis_index++)
    {
        uint16_t packed = (uint16_t)raw[(uint8_t)(axis_index * 2U)] |
            (uint16_t)((uint16_t)raw[(uint8_t)(axis_index * 2U + 1U)] << 8U);
        int16_t signed_packed = (int16_t)packed;
        int16_t sample = (int16_t)(signed_packed >> APP_ACCELEROMETER_OUTPUT_SHIFT);
        acceleration_g[axis_index] =
            ((float)sample * APP_ACCELEROMETER_SENSITIVITY_MG_PER_LSB) / 1000.0F;
    }

    return true;
}

/**
 * @brief Reads calibrated MCU die temperature in degrees Celsius.
 *
 * VREFINT provides measured VDDA for the per-device STM32F4 factory
 * temperature-calibration calculation.
 *
 * @param[out] temperature_c Destination for approximate die temperature in °C.
 * @return true when both ADC samples and calculated VDDA are valid; otherwise
 *         false.
 * @pre maintenance_sensor_init() returned true.
 * @note This is die temperature, not ambient temperature.
 */
bool maintenance_sensor_read_temperature(float *temperature_c)
{
    uint16_t temperature_raw = 0U;
    uint16_t reference_raw = 0U;
    uint32_t supply_mv;
    int32_t temperature_result_c;

    if ((temperature_c == NULL) || !sensor_is_ready ||
        !sensor_adc_is_initialized)
    {
        return false;
    }

    if (!sensor_adc_convert(LL_ADC_CHANNEL_VREFINT, &reference_raw) ||
        !sensor_adc_convert(LL_ADC_CHANNEL_TEMPSENSOR, &temperature_raw) ||
        (reference_raw == 0U) || (temperature_raw == 0U))
    {
        return false;
    }

    supply_mv = (VREFINT_CAL_VREF * (uint32_t)(*VREFINT_CAL_ADDR)) /
                (uint32_t)reference_raw;
    if ((supply_mv < APP_ADC_VDDA_MIN_MV) ||
        (supply_mv > APP_ADC_VDDA_MAX_MV))
    {
        return false;
    }

    temperature_result_c = (int32_t)__LL_ADC_CALC_TEMPERATURE(
        supply_mv, temperature_raw, LL_ADC_RESOLUTION_12B);
    if ((temperature_result_c < APP_ADC_TEMPERATURE_MIN_C) ||
        (temperature_result_c > APP_ADC_TEMPERATURE_MAX_C))
    {
        return false;
    }

    *temperature_c = (float)temperature_result_c;
    return true;
}

/* Configures SPI1, AF5 pin routing and active-low PE3 chip select once. */
static bool sensor_spi_init(void)
{
    if (sensor_spi_is_initialized)
    {
        return true;
    }

    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA |
                             LL_AHB1_GRP1_PERIPH_GPIOE);
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);

    LL_GPIO_SetOutputPin(APP_ACCELEROMETER_CS_GPIO_PORT,
                         APP_ACCELEROMETER_CS_GPIO_PIN);
    LL_GPIO_SetPinMode(APP_ACCELEROMETER_CS_GPIO_PORT,
                       APP_ACCELEROMETER_CS_GPIO_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(APP_ACCELEROMETER_CS_GPIO_PORT,
                             APP_ACCELEROMETER_CS_GPIO_PIN,
                             LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(APP_ACCELEROMETER_CS_GPIO_PORT,
                        APP_ACCELEROMETER_CS_GPIO_PIN,
                        LL_GPIO_SPEED_FREQ_MEDIUM);
    LL_GPIO_SetPinPull(APP_ACCELEROMETER_CS_GPIO_PORT,
                       APP_ACCELEROMETER_CS_GPIO_PIN, LL_GPIO_PULL_NO);

    LL_GPIO_SetPinMode(APP_ACCELEROMETER_SCK_GPIO_PORT,
                       APP_ACCELEROMETER_SCK_GPIO_PIN, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinMode(APP_ACCELEROMETER_MISO_GPIO_PORT,
                       APP_ACCELEROMETER_MISO_GPIO_PIN, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinMode(APP_ACCELEROMETER_MOSI_GPIO_PORT,
                       APP_ACCELEROMETER_MOSI_GPIO_PIN, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinOutputType(APP_ACCELEROMETER_SCK_GPIO_PORT,
                             APP_ACCELEROMETER_SCK_GPIO_PIN,
                             LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinOutputType(APP_ACCELEROMETER_MISO_GPIO_PORT,
                             APP_ACCELEROMETER_MISO_GPIO_PIN,
                             LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinOutputType(APP_ACCELEROMETER_MOSI_GPIO_PORT,
                             APP_ACCELEROMETER_MOSI_GPIO_PIN,
                             LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(APP_ACCELEROMETER_SCK_GPIO_PORT,
                        APP_ACCELEROMETER_SCK_GPIO_PIN,
                        LL_GPIO_SPEED_FREQ_MEDIUM);
    LL_GPIO_SetPinSpeed(APP_ACCELEROMETER_MISO_GPIO_PORT,
                        APP_ACCELEROMETER_MISO_GPIO_PIN,
                        LL_GPIO_SPEED_FREQ_MEDIUM);
    LL_GPIO_SetPinSpeed(APP_ACCELEROMETER_MOSI_GPIO_PORT,
                        APP_ACCELEROMETER_MOSI_GPIO_PIN,
                        LL_GPIO_SPEED_FREQ_MEDIUM);
    LL_GPIO_SetPinPull(APP_ACCELEROMETER_SCK_GPIO_PORT,
                       APP_ACCELEROMETER_SCK_GPIO_PIN, LL_GPIO_PULL_DOWN);
    LL_GPIO_SetPinPull(APP_ACCELEROMETER_MISO_GPIO_PORT,
                       APP_ACCELEROMETER_MISO_GPIO_PIN, LL_GPIO_PULL_DOWN);
    LL_GPIO_SetPinPull(APP_ACCELEROMETER_MOSI_GPIO_PORT,
                       APP_ACCELEROMETER_MOSI_GPIO_PIN, LL_GPIO_PULL_DOWN);
    LL_GPIO_SetAFPin_0_7(APP_ACCELEROMETER_SCK_GPIO_PORT,
                         APP_ACCELEROMETER_SCK_GPIO_PIN, LL_GPIO_AF_5);
    LL_GPIO_SetAFPin_0_7(APP_ACCELEROMETER_MISO_GPIO_PORT,
                         APP_ACCELEROMETER_MISO_GPIO_PIN, LL_GPIO_AF_5);
    LL_GPIO_SetAFPin_0_7(APP_ACCELEROMETER_MOSI_GPIO_PORT,
                         APP_ACCELEROMETER_MOSI_GPIO_PIN, LL_GPIO_AF_5);

    LL_SPI_Disable(SPI1);
    LL_SPI_SetMode(SPI1, LL_SPI_MODE_MASTER);
    LL_SPI_SetTransferDirection(SPI1, LL_SPI_FULL_DUPLEX);
    LL_SPI_SetDataWidth(SPI1, LL_SPI_DATAWIDTH_8BIT);
    LL_SPI_SetClockPolarity(SPI1, LL_SPI_POLARITY_LOW);
    LL_SPI_SetClockPhase(SPI1, LL_SPI_PHASE_1EDGE);
    LL_SPI_SetNSSMode(SPI1, LL_SPI_NSS_SOFT);
    LL_SPI_SetBaudRatePrescaler(SPI1, LL_SPI_BAUDRATEPRESCALER_DIV16);
    LL_SPI_SetTransferBitOrder(SPI1, LL_SPI_MSB_FIRST);
    LL_SPI_Enable(SPI1);

    sensor_spi_is_initialized = true;
    return true;
}

/* Sends and receives one SPI byte with finite TXE, RXNE and BSY waits. */
static bool sensor_spi_exchange(uint8_t tx_byte, uint8_t *rx_byte)
{
    uint32_t started_ms = board_io_millis();
    uint8_t result;

    while (LL_SPI_IsActiveFlag_TXE(SPI1) == 0U)
    {
        if ((uint32_t)(board_io_millis() - started_ms) >=
            SENSOR_SPI_TIMEOUT_MS)
        {
            return false;
        }
    }

    LL_SPI_TransmitData8(SPI1, tx_byte);
    started_ms = board_io_millis();
    while (LL_SPI_IsActiveFlag_RXNE(SPI1) == 0U)
    {
        if ((uint32_t)(board_io_millis() - started_ms) >=
            SENSOR_SPI_TIMEOUT_MS)
        {
            return false;
        }
    }

    result = LL_SPI_ReceiveData8(SPI1);
    if (rx_byte != NULL)
    {
        *rx_byte = result;
    }

    return sensor_spi_wait_idle();
}

/* Waits until the final SPI bit is shifted out, bounded by the configured timeout. */
static bool sensor_spi_wait_idle(void)
{
    uint32_t started_ms = board_io_millis();

    while (LL_SPI_IsActiveFlag_BSY(SPI1) != 0U)
    {
        if ((uint32_t)(board_io_millis() - started_ms) >=
            SENSOR_SPI_TIMEOUT_MS)
        {
            return false;
        }
    }

    return true;
}

/* Reads up to the six-byte acceleration payload in one CS-bounded transaction. */
static bool sensor_spi_read(uint8_t address, uint8_t *data, uint16_t size)
{
    uint16_t byte_index;
    uint8_t command = (uint8_t)(address | APP_ACCELEROMETER_READ_COMMAND);
    bool is_valid;

    if ((data == NULL) || (size == 0U) ||
        (size > SENSOR_SPI_MAX_TRANSFER_BYTES) || !sensor_spi_init())
    {
        return false;
    }

    if (size > 1U)
    {
        command |= (uint8_t)APP_ACCELEROMETER_MULTI_BYTE_COMMAND;
    }

    LL_GPIO_ResetOutputPin(APP_ACCELEROMETER_CS_GPIO_PORT,
                           APP_ACCELEROMETER_CS_GPIO_PIN);
    is_valid = sensor_spi_exchange(command, NULL);
    for (byte_index = 0U; (byte_index < size) && is_valid; byte_index++)
    {
        is_valid = sensor_spi_exchange(0U, &data[byte_index]);
    }

    if (!sensor_spi_wait_idle())
    {
        is_valid = false;
    }
    LL_GPIO_SetOutputPin(APP_ACCELEROMETER_CS_GPIO_PORT,
                         APP_ACCELEROMETER_CS_GPIO_PIN);
    return is_valid;
}

/* Writes sensor configuration bytes in the Cube BSP command format. */
static bool sensor_spi_write(uint8_t address, const uint8_t *data, uint16_t size)
{
    uint16_t byte_index;
    uint8_t command = address;
    bool is_valid;

    if ((data == NULL) || (size == 0U) ||
        (size > SENSOR_SPI_MAX_TRANSFER_BYTES) || !sensor_spi_init())
    {
        return false;
    }

    if (size > 1U)
    {
        command |= (uint8_t)APP_ACCELEROMETER_MULTI_BYTE_COMMAND;
    }

    LL_GPIO_ResetOutputPin(APP_ACCELEROMETER_CS_GPIO_PORT,
                           APP_ACCELEROMETER_CS_GPIO_PIN);
    is_valid = sensor_spi_exchange(command, NULL);
    for (byte_index = 0U; (byte_index < size) && is_valid; byte_index++)
    {
        is_valid = sensor_spi_exchange(data[byte_index], NULL);
    }

    if (!sensor_spi_wait_idle())
    {
        is_valid = false;
    }
    LL_GPIO_SetOutputPin(APP_ACCELEROMETER_CS_GPIO_PORT,
                         APP_ACCELEROMETER_CS_GPIO_PIN);
    return is_valid;
}

/* Verifies WHO_AM_I, then applies the accepted 100 Hz, ±4 g sensor setup. */
static bool sensor_lis3dsh_init(void)
{
    uint8_t device_id = 0U;
    uint8_t control_register_4 = (uint8_t)(APP_ACCELEROMETER_DATA_RATE_REGISTER |
                                           APP_ACCELEROMETER_ENABLE_AXES_REGISTER |
                                           APP_ACCELEROMETER_BDU_BIT);
    uint8_t control_register_6 = APP_ACCELEROMETER_CTRL6_VALUE;
    uint8_t control_register_5 = (uint8_t)(APP_ACCELEROMETER_FULL_SCALE_REGISTER |
                                           APP_ACCELEROMETER_FILTER_REGISTER);

    if (!sensor_spi_read(APP_ACCELEROMETER_ID_REGISTER, &device_id, 1U) ||
        (device_id != APP_ACCELEROMETER_ID_VALUE))
    {
        return false;
    }

    if (!sensor_spi_write(APP_ACCELEROMETER_CTRL4_REGISTER, &control_register_4, 1U) ||
        !sensor_spi_write(APP_ACCELEROMETER_CTRL5_REGISTER, &control_register_5, 1U) ||
        !sensor_spi_write(APP_ACCELEROMETER_CTRL6_REGISTER, &control_register_6, 1U))
    {
        return false;
    }

    return true;
}

/* Configures ADC1 internal channels and gives them their datasheet settling time. */
static bool sensor_adc_init(void)
{
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_ADC1);
    LL_ADC_SetCommonClock(__LL_ADC_COMMON_INSTANCE(ADC1),
                          LL_ADC_CLOCK_SYNC_PCLK_DIV4);
    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC1),
        LL_ADC_PATH_INTERNAL_TEMPSENSOR | LL_ADC_PATH_INTERNAL_VREFINT);
    LL_ADC_SetResolution(ADC1, LL_ADC_RESOLUTION_12B);
    LL_ADC_SetDataAlignment(ADC1, LL_ADC_DATA_ALIGN_RIGHT);
    LL_ADC_SetSequencersScanMode(ADC1, LL_ADC_SEQ_SCAN_DISABLE);
    LL_ADC_REG_SetTriggerSource(ADC1, LL_ADC_REG_TRIG_SOFTWARE);
    LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_SINGLE);
    LL_ADC_REG_SetSequencerLength(ADC1, LL_ADC_REG_SEQ_SCAN_DISABLE);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_TEMPSENSOR,
                                  LL_ADC_SAMPLINGTIME_480CYCLES);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_VREFINT,
                                  LL_ADC_SAMPLINGTIME_480CYCLES);
    LL_ADC_Enable(ADC1);
    sensor_adc_stabilization_delay();
    sensor_adc_is_initialized = true;
    return true;
}

/* Runs one selected internal channel conversion and checks the completion bound. */
static bool sensor_adc_convert(uint32_t channel, uint16_t *value)
{
    uint32_t started_ms;

    if ((value == NULL) || !sensor_adc_is_initialized)
    {
        return false;
    }

    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, channel);
    LL_ADC_ClearFlag_EOCS(ADC1);
    LL_ADC_REG_StartConversionSWStart(ADC1);
    started_ms = board_io_millis();

    while (LL_ADC_IsActiveFlag_EOCS(ADC1) == 0U)
    {
        if ((uint32_t)(board_io_millis() - started_ms) >=
            SENSOR_ADC_TIMEOUT_MS)
        {
            return false;
        }
    }

    *value = LL_ADC_REG_ReadConversionData12(ADC1);
    return true;
}

/* Delays for the LL driver's documented internal temperature stabilization. */
static void sensor_adc_stabilization_delay(void)
{
    volatile uint32_t remaining_cycles =
        (SystemCoreClock / 1000000U) * LL_ADC_DELAY_TEMPSENSOR_STAB_US;

    while (remaining_cycles > 0U)
    {
        remaining_cycles--;
        __NOP();
    }
}
