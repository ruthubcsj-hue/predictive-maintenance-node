/* Cooperative vibration and internal-temperature monitoring application. */
#include "app.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "app_config.h"
#include "board_io.h"
#include "console.h"
#include "maintenance_sensor.h"

#define APP_G_FORCE_SAMPLES     (APP_VIBRATION_SAMPLES_PER_WINDOW)
#define APP_AXIS_COUNT          (3U)
#define APP_SERIAL_LINE_SIZE    (80U)

static bool app_baseline_is_ready;
static bool app_fault_is_active;
static bool app_anomaly_is_active;
static bool app_has_valid_window;
static uint8_t app_baseline_windows;
static uint8_t app_temperature_high_windows;
static uint8_t app_healthy_windows;
static uint16_t app_window_sample_count;
static uint16_t app_window_sample_index;
static uint32_t app_next_sample_deadline_ms;
static uint32_t app_next_status_deadline_ms;
static float app_axis_samples[APP_AXIS_COUNT][APP_G_FORCE_SAMPLES];
static float app_baseline_sum_g;
static float app_baseline_rms_g;
static float app_vibration_rms_g;
static float app_temperature_c;

static bool app_time_reached(uint32_t now_ms, uint32_t deadline_ms);
static bool app_collect_sample(void);
static bool app_finish_vibration_window(void);
static float app_compute_detrended_rms(void);
static void app_update_status(uint32_t now_ms);
static void app_apply_status_outputs(void);
static void app_report_status(uint32_t now_ms);
static const char *app_status_name(void);
static void app_enter_fault(void);

/**
 * @brief Runs the 100 Hz sample, one-second RMS and status reporting loop.
 *
 * Invalid sensor transactions stop acquisition, report FAULT and leave both
 * indicators off. Scheduling uses wrap-safe SysTick time and discards missed
 * sample deadlines rather than synthesizing samples.
 */
static void app_run_monitoring_loop(void)
{
    for (;;)
    {
        uint32_t now_ms = board_io_millis();

        if (app_time_reached(now_ms, app_next_sample_deadline_ms))
        {
            app_next_sample_deadline_ms += APP_VIBRATION_SAMPLE_PERIOD_MS;
            if (!app_collect_sample())
            {
                app_enter_fault();
            }
            else if (app_window_sample_count == APP_G_FORCE_SAMPLES)
            {
                if (!app_finish_vibration_window())
                {
                    app_enter_fault();
                }
                else
                {
                    app_update_status(now_ms);
                }
            }
        }

        if (app_time_reached(now_ms, app_next_status_deadline_ms))
        {
            app_next_status_deadline_ms += APP_STATUS_REPORT_PERIOD_MS;
            app_report_status(now_ms);
        }

        app_apply_status_outputs();
        board_io_wait_for_interrupt();
    }
}

/* Tests an unsigned deadline using half-range wrap-safe time comparison. */
static bool app_time_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

/* Reads one 3-axis sample and appends it to the open one-second buffer. */
static bool app_collect_sample(void)
{
    float acceleration_g[APP_AXIS_COUNT];
    bool sample_is_valid;

    sample_is_valid = maintenance_sensor_read_acceleration(acceleration_g);
    if (!sample_is_valid)
    {
        return false;
    }

    app_axis_samples[0][app_window_sample_index] = acceleration_g[0];
    app_axis_samples[1][app_window_sample_index] = acceleration_g[1];
    app_axis_samples[2][app_window_sample_index] = acceleration_g[2];
    app_window_sample_index++;
    app_window_sample_count++;
    if (app_window_sample_index == APP_G_FORCE_SAMPLES)
    {
        app_window_sample_index = 0U;
    }

    return true;
}

/* Converts a complete acceleration window to RMS and samples die temperature. */
static bool app_finish_vibration_window(void)
{
    float next_temperature_c;

    app_vibration_rms_g = app_compute_detrended_rms();
    if (!maintenance_sensor_read_temperature(&next_temperature_c))
    {
        return false;
    }

    app_temperature_c = next_temperature_c;
    app_window_sample_count = 0U;
    app_has_valid_window = true;
    return true;
}

/* Removes each axis mean so gravity and static orientation are excluded. */
static float app_compute_detrended_rms(void)
{
    uint16_t sample_index;
    uint8_t axis_index;
    float axis_means[APP_AXIS_COUNT] = {0.0F, 0.0F, 0.0F};
    float sum_squared_deviation = 0.0F;
    uint32_t total_axis_samples =
        APP_VIBRATION_SAMPLES_PER_WINDOW * APP_AXIS_COUNT;

    for (axis_index = 0U; axis_index < APP_AXIS_COUNT; axis_index++)
    {
        for (sample_index = 0U; sample_index < APP_VIBRATION_SAMPLES_PER_WINDOW;
             sample_index++)
        {
            axis_means[axis_index] += app_axis_samples[axis_index][sample_index];
        }
        axis_means[axis_index] /= (float)APP_VIBRATION_SAMPLES_PER_WINDOW;
    }

    for (axis_index = 0U; axis_index < APP_AXIS_COUNT; axis_index++)
    {
        for (sample_index = 0U; sample_index < APP_VIBRATION_SAMPLES_PER_WINDOW;
             sample_index++)
        {
            float deviation_g = app_axis_samples[axis_index][sample_index] -
                                axis_means[axis_index];
            sum_squared_deviation += deviation_g * deviation_g;
        }
    }

    return __builtin_sqrtf(sum_squared_deviation / (float)total_axis_samples);
}

/* Learns the ten-window baseline and updates vibration/temperature latches. */
static void app_update_status(uint32_t now_ms)
{
    bool vibration_is_high = false;
    bool temperature_is_high;
    bool window_is_healthy;

    if (!app_baseline_is_ready)
    {
        app_baseline_sum_g += app_vibration_rms_g;
        app_baseline_windows++;
        if (app_baseline_windows >= APP_BASELINE_WINDOW_COUNT)
        {
            app_baseline_rms_g = app_baseline_sum_g /
                                 (float)APP_BASELINE_WINDOW_COUNT;
            app_baseline_is_ready = true;
        }
    }
    else
    {
        float baseline_floor_g = APP_ACCELEROMETER_LSB_G;
        float comparison_baseline_g = app_baseline_rms_g;

        if (comparison_baseline_g < baseline_floor_g)
        {
            comparison_baseline_g = baseline_floor_g;
        }
        vibration_is_high = app_vibration_rms_g >
            (APP_VIBRATION_ANOMALY_MULTIPLIER * comparison_baseline_g);
    }

    temperature_is_high = app_temperature_c >
                          APP_TEMPERATURE_ANOMALY_THRESHOLD_C;
    if (temperature_is_high)
    {
        if (app_temperature_high_windows < APP_TEMPERATURE_ANOMALY_WINDOW_COUNT)
        {
            app_temperature_high_windows++;
        }
    }
    else
    {
        app_temperature_high_windows = 0U;
    }

    if (vibration_is_high ||
        (app_temperature_high_windows >= APP_TEMPERATURE_ANOMALY_WINDOW_COUNT))
    {
        app_anomaly_is_active = true;
        app_healthy_windows = 0U;
    }
    else if (app_anomaly_is_active && app_baseline_is_ready)
    {
        window_is_healthy = !vibration_is_high && !temperature_is_high;
        if (window_is_healthy)
        {
            if (app_healthy_windows < APP_ANOMALY_CLEAR_WINDOW_COUNT)
            {
                app_healthy_windows++;
            }
            if (app_healthy_windows >= APP_ANOMALY_CLEAR_WINDOW_COUNT)
            {
                app_anomaly_is_active = false;
                app_healthy_windows = 0U;
            }
        }
        else
        {
            app_healthy_windows = 0U;
        }
    }

    (void)now_ms;
}

/* Selects an LED state from the baseline, fault and anomaly state. */
static void app_apply_status_outputs(void)
{
    bool green_is_on = false;
    bool red_is_on = false;

    if (!app_fault_is_active && app_baseline_is_ready)
    {
        if (app_anomaly_is_active)
        {
            red_is_on = true;
        }
        else
        {
            green_is_on = true;
        }
    }
    board_io_set_status_leds(green_is_on, red_is_on);
}

/* Emits one CSV record each second; unavailable readings are never fabricated. */
static void app_report_status(uint32_t now_ms)
{
    char report_line[APP_SERIAL_LINE_SIZE];
    int report_length;

    (void)now_ms;
    if (app_fault_is_active)
    {
        (void)printf("FAULT,NA,NA\r\n");
        return;
    }

    if (!app_has_valid_window)
    {
        (void)printf("%s,NA,NA\r\n", app_status_name());
        return;
    }

    report_length = snprintf(report_line, sizeof(report_line), "%s,%.3f,%.1f\r\n",
                             app_status_name(), (double)app_vibration_rms_g,
                             (double)app_temperature_c);
    if ((report_length > 0) &&
        ((size_t)report_length < sizeof(report_line)))
    {
        (void)printf("%s", report_line);
    }
}

/* Maps private status state to the exact serial status tokens. */
static const char *app_status_name(void)
{
    const char *status_name = "CALIBRATING";

    if (app_anomaly_is_active)
    {
        status_name = "ANOMALY";
    }
    else if (app_baseline_is_ready)
    {
        status_name = "NORMAL";
    }

    return status_name;
}

/* Stops normal indications after any failed sensor or temperature transaction. */
static void app_enter_fault(void)
{
    app_fault_is_active = true;
    app_baseline_is_ready = false;
    app_apply_status_outputs();
}

void app_start(void)
{
    board_io_init();
    console_init();

    if (!maintenance_sensor_init())
    {
        app_enter_fault("initialization");
    }
    else
    {
        app_next_sample_deadline_ms = board_io_millis() +
                                      APP_VIBRATION_SAMPLE_PERIOD_MS;
        app_next_status_deadline_ms = board_io_millis() +
                                      APP_STATUS_REPORT_PERIOD_MS;
    }

    app_report_status(board_io_millis());
    app_run_monitoring_loop();
}
