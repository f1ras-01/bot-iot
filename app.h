/*
 * app.h
 * ----------------------------------------------------------------------------
 * Application layer: shared global state, the ADC reporting helper, and the
 * interrupt service routines that tie several peripherals together.
 *
 * The ISRs now do flag-setting work ONLY. They never call blocking routines
 * such as USART2_SendString(). All printing/parsing happens in the main loop.
 * ----------------------------------------------------------------------------
 */
#ifndef APP_H
#define APP_H

#include "stm32f4xx.h"
#include "config.h"

/* ----- Shared global state (defined in app.c) ----- */

/* Motor control */
extern volatile int speed;                  /* PWM duty (0..999) */

/* Temperature */
extern volatile float temperature;

/* ADC values storage */
extern volatile uint16_t adc_values[3];     /* POT1, POT2, POT3 */
extern volatile uint8_t  channel_index;     /* Current channel (0,1,2) */
extern volatile uint8_t  adc_ready;         /* Flag: all 3 values ready */
extern volatile uint8_t  adc_running;       /* 0 = stopped, 1 = running */

/* Bluetooth */
extern char             rxBuffer[RX_BUFFER_SIZE];
extern volatile uint8_t bufferIndex;
extern volatile uint8_t dataReady;

/* ----- ISR-to-mainloop event flags (set in an ISR, handled in main) ----- */

/* Set by the button ISR; main loop performs the ADC start/stop + messaging. */
extern volatile uint8_t button_event;

/* ----- Motor command failsafe ----- */

/* Millis() timestamp of the last received drive command. The main loop stops
 * the motors if no command arrives within MOTOR_CMD_TIMEOUT_MS. */
extern volatile uint32_t last_cmd_time;

/* ----- Helpers ----- */

/* Format and send the 3 ADC readings over UART (call from main loop only). */
void SendADCValues(void);

#endif /* APP_H */
