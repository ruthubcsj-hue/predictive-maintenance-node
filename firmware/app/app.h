/* Application entry point for the predictive-maintenance node. */
#ifndef APP_H
#define APP_H

/**
 * @brief Starts periodic vibration and MCU-temperature monitoring.
 *
 * Initializes board I/O, the UART console, onboard sensor adapters, and then
 * runs cooperative 100 Hz sampling, anomaly-state, indicator, and CSV reporting.
 *
 * @pre Startup has configured the system clock and 1 ms SysTick.
 * @post Does not return during normal operation.
 */
void app_start(void);

#endif /* APP_H */
