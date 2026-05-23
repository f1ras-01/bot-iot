/*
 * app.c
 * ----------------------------------------------------------------------------
 * Application layer: shared global state, the ADC reporting helper, and the
 * interrupt service routines (EXTI0, ADC, USART2).
 *
 * Design rule for ISRs in this file: do the minimum (read a register, store a
 * value, set a flag) and return. No ISR calls a blocking function. Anything
 * that prints or parses is deferred to the main loop, which keeps interrupt
 * latency short and avoids losing USART bytes during long UART transmissions.
 * ----------------------------------------------------------------------------
 */
#include "app.h"
#include "system.h"
#include "usart.h"
#include <stdio.h>

/* ============================================================================
 * GLOBAL VARIABLES
 * ==========================================================================*/

/* Motor control */
volatile int speed = 250;  // Default 25% speed

/* Temperature */
volatile float temperature = 0.0f;

/* ADC values storage */
volatile uint16_t adc_values[3] = {0, 0, 0};  // POT1, POT2, POT3
volatile uint8_t  channel_index = 0;          // Current channel (0, 1, 2)
volatile uint8_t  adc_ready = 0;              // Flag: All 3 values ready
volatile uint8_t  adc_running = 0;            // 0 = stopped, 1 = running

/* Bluetooth */
char             rxBuffer[RX_BUFFER_SIZE];
volatile uint8_t bufferIndex = 0;
volatile uint8_t dataReady = 0;

/* ISR-to-mainloop event flags */
volatile uint8_t button_event = 0;

/* Motor command failsafe timestamp */
volatile uint32_t last_cmd_time = 0;

/* ============================================================================
 * SendADCValues - Format and send ADC values via UART
 * ----------------------------------------------------------------------------
 * Called from the main loop only (it blocks on UART). Voltage is computed with
 * integer math; no floating-point printf is used anywhere in this project.
 * ==========================================================================*/
void SendADCValues(void)
{
    char msg[64];

    /* Snapshot the volatile values once, so the three printed readings are
     * consistent even if the ADC ISR updates the array mid-function. */
    uint16_t v0 = adc_values[0];
    uint16_t v1 = adc_values[1];
    uint16_t v2 = adc_values[2];

    USART2_SendString("\r\n===== ADC READINGS =====\r\n");

    sprintf(msg, "POT1 (PA1): %u (%u mV)\r\n", v0, (unsigned)((v0 * 3300U) / 4095U));
    USART2_SendString(msg);

    sprintf(msg, "POT2 (PA4): %u (%u mV)\r\n", v1, (unsigned)((v1 * 3300U) / 4095U));
    USART2_SendString(msg);

    sprintf(msg, "POT3 (PA5): %u (%u mV)\r\n", v2, (unsigned)((v2 * 3300U) / 4095U));
    USART2_SendString(msg);

    USART2_SendString("========================\r\n");
}

/* ============================================================================
 * EXTI0_IRQHandler - Button press: just record the event
 * ----------------------------------------------------------------------------
 * Previously this handler started/stopped the ADC and printed several lines
 * over UART (~tens of ms of blocking inside an interrupt). It now only clears
 * the pending bit and sets a flag; the main loop does the real work.
 * ==========================================================================*/
void EXTI0_IRQHandler(void)
{
    if (EXTI->PR & (1 << 0))      // Check EXTI0 pending flag
    {
        EXTI->PR |= (1 << 0);     // Clear pending flag (write 1 to clear)
        button_event = 1;         // Defer handling to the main loop
    }
}

/* ============================================================================
 * ADC_IRQHandler - Store ADC conversion results
 * ==========================================================================*/
void ADC_IRQHandler(void)
{
    if (ADC1->SR & (1 << 1))  // EOC flag
    {
        adc_values[channel_index] = ADC1->DR;  // Read DR (clears EOC)

        channel_index++;

        if (channel_index >= 3)
        {
            channel_index = 0;  // Reset for next cycle
            adc_ready = 1;      // Set flag: data ready to send
        }
    }
}

/* ============================================================================
 * USART2_IRQHandler - Receive Bluetooth commands
 * ----------------------------------------------------------------------------
 * Still flag-only: bytes are buffered, and an idle line raises dataReady.
 * No processing is done here.
 * ==========================================================================*/
void USART2_IRQHandler(void)
{
    // RXNE: Receive data register not empty
    if (USART2->SR & (1 << 5))
    {
        char c = (char)USART2->DR;

        // Ignore newline/carriage return
        if (c == '\n' || c == '\r')
            return;

        // Add character to buffer (drops extra bytes when full)
        if (bufferIndex < sizeof(rxBuffer) - 1)
            rxBuffer[bufferIndex++] = c;
    }

    // IDLE: Idle line detected (end of transmission)
    if (USART2->SR & (1 << 4))
    {
        volatile uint32_t tmp = USART2->SR;
        tmp = USART2->DR;
        (void)tmp;  // Clear IDLE flag

        rxBuffer[bufferIndex] = '\0';  // Null-terminate
        dataReady = 1;  // Set flag
    }
}
