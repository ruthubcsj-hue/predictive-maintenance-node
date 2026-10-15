/* Vendor-neutral initialization contract for the application status console. */
#ifndef CONSOLE_H
#define CONSOLE_H

/**
 * @brief Initializes the configured console transport for application logs.
 *
 * The platform implementation configures its transmit path before the
 * application emits any printf-backed logger output.
 *
 * @pre SystemClock_Config() and board_io_init() have completed.
 * @post stdout and stderr can be sent by the platform console backend.
 */
void console_init(void);

#endif /* CONSOLE_H */
