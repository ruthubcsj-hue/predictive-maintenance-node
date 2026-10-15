/* Vendor-neutral measurement contract for onboard maintenance sensors. */
#ifndef MAINTENANCE_SENSOR_H
#define MAINTENANCE_SENSOR_H

#include <stdbool.h>

/**
 * @brief Initializes the onboard vibration sensor and MCU temperature ADC path.
 *
 * The implementation validates the LIS3DSH identity and prepares ADC1 internal
 * channels used to measure the MCU die temperature.
 *
 * @return true when both acquisition paths are initialized; otherwise false.
 * @pre The system clock and board millisecond timebase are active.
 * @post A false result means measurements must not be reported as valid.
 */
bool maintenance_sensor_init(void);

/**
 * @brief Reads the current LIS3DSH three-axis acceleration in g.
 *
 * @param[out] acceleration_g Destination array ordered X, Y, Z; values are in g.
 * @return true when all three values are valid; otherwise false.
 * @pre maintenance_sensor_init() returned true.
 * @post The array is updated only when true is returned.
 */
bool maintenance_sensor_read_acceleration(float acceleration_g[3]);

/**
 * @brief Reads the calibrated MCU internal temperature in degrees Celsius.
 *
 * ADC1 samples the internal temperature sensor and VREFINT so the conversion
 * accounts for the measured analog supply and device factory calibration.
 *
 * @param[out] temperature_c Destination for approximate MCU die temperature in °C.
 * @return true when the ADC samples and calibration are valid; otherwise false.
 * @pre maintenance_sensor_init() returned true.
 * @post The value is updated only when true is returned.
 * @note This is MCU die temperature, not ambient temperature.
 */
bool maintenance_sensor_read_temperature(float *temperature_c);

#endif /* MAINTENANCE_SENSOR_H */
