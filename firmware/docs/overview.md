# STM32F4DISCOVERY Predictive Maintenance Node

This firmware monitors the board's LIS3DSH accelerometer and the STM32F407 internal temperature sensor. The foreground loop polls acceleration every 10 ms, calculates a one-second three-axis detrended RMS, learns a baseline from ten completed windows, and writes one status record per second over USART2.

## Hardware and behavior

- **Accelerometer:** onboard LIS3DSH on SPI1: SCK PA5, MISO PA6, MOSI PA7, active-low CS PE3. The board knowledge record grounds those pins. The CubeF4 Discovery BSP confirms SPI1 mode 0 and the 100 Hz, four-wire LIS3DSH configuration; ±4 g and 800 Hz bandwidth are selected as configured in the accepted implementation plan.
- **Temperature:** ADC1 internal temperature sensor and VREFINT. The STM32F4 factory calibration data and measured VDDA are used for an approximate MCU die temperature, not ambient temperature.
- **Vibration metric:** remove each axis's mean over 100 samples, then compute RMS of the combined three-axis dynamic acceleration in g. Static gravity is not part of this RMS.
- **Baseline:** arithmetic mean of the first ten complete one-second RMS windows. The status is `CALIBRATING` until those windows are collected; both indicators remain off unless an anomaly is latched.
- **Anomaly policy:** vibration RMS greater than 2× baseline (using a one-LSB floor for a near-zero baseline), or temperature above 70 °C for three consecutive one-second readings. An active anomaly clears after three consecutive windows that satisfy both health checks.
- **Indicators:** blue LD6 = calibrating (first ten windows); green LD4 = normal; orange LD3 = warning (vibration RMS above 1.5x baseline); red LD5 = anomaly, with an optional external active buzzer on PA1 beeping at 2.5 Hz; orange + blue together = sensor/ADC acquisition fault.
- **Recalibrate:** pressing the user button B1 (PA0) restarts baseline learning and prints `RECALIBRATING,NA,NA`.
- **Serial:** USART2 TX PA2 at 115200 baud, 8-N-1. Lines have `STATUS,RMS_g,TEMP_C` fields, e.g. `NORMAL,0.025,25.3`; calibration reports `CALIBRATING`, an elevated reading reports `WARNING`, and invalid acquisition reports `FAULT,NA,NA`.

## Source responsibilities

- `firmware/app/app.c` owns cooperative timing, one-second detrended RMS, ten-window baseline, anomaly persistence/clear policy, LED policy and CSV reporting.
- `firmware/interfaces/maintenance_sensor.h` defines vendor-neutral acquisition for acceleration in g and die temperature in °C.
- `firmware/platforms/stm32/maintenance_sensor_stm32.c` configures SPI1/ADC1, implements the Cube LIS3DSH `ACCELERO_IO_*` callback seam, and reads calibrated temperature/VREFINT.
- `firmware/interfaces/board_io.h` defines status LEDs and monotonic time; `firmware/platforms/stm32/board_io_stm32.c` implements LD4/LD5 and the SysTick clock.
- `firmware/configs/app_config.h` stores board pin mappings and named periods, timeouts and thresholds; `firmware/bsp/board.h` records the active-high LED facts.
- `Core/Src/main.c` stays startup glue and calls `app_start()` after clock and 1 ms SysTick initialization.

## Build and verification

Build with the project's CMake + Ninja STM32 workflow and deploy using its ST-Link workflow. Connect a terminal to USART2 TX PA2 at 115200 baud, 8-N-1. After the ten-second baseline, leave the board still and check for `NORMAL`; then shake it and check for `ANOMALY` within three seconds. Stop motion and check for recovery after three healthy windows. Temperature persistence and sensor fault behavior require controlled input or safe fault-injection checks. Built with CMake + Ninja (arm-none-eabi-gcc). Verified on an STM32F407G-DISC1 for the LED behavior; the temperature alarm, sensor-fault path and serial output were not verified.
