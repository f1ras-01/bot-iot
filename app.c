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

/* ADC values storage. The DMA writes each conversion straight into this
 * array; no software channel index is needed. */
volatile uint16_t adc_values[3] = {0, 0, 0};  // POT1, POT2, POT3
volatile uint8_t  adc_ready = 0;              // Flag: All 3 values ready
volatile uint8_t  adc_running = 0;            // 0 = stopped, 1 = running

/* Bluetooth */
char             rxBuffer[RX_BUFFER_SIZE];
volatile uint8_t bufferIndex = 0;
volatile uint8_t dataReady = 0;

/* ISR-to-mainloop event flags */
volatile uint8_t button_event = 0;

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

    BT_SendString("\r\n===== ADC READINGS =====\r\n");

    sprintf(msg, "POT1 (PA1): %u (%u mV)\r\n", v0, (unsigned)((v0 * 3300U) / 4095U));
    BT_SendString(msg);

    sprintf(msg, "POT2 (PA4): %u (%u mV)\r\n", v1, (unsigned)((v1 * 3300U) / 4095U));
    BT_SendString(msg);

    sprintf(msg, "POT3 (PA5): %u (%u mV)\r\n", v2, (unsigned)((v2 * 3300U) / 4095U));
    BT_SendString(msg);

    BT_SendString("========================\r\n");
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

/* ----------------------------------------------------------------------------
 * NOTE: the ADC no longer uses a per-conversion interrupt. Results are moved
 * by DMA2 Stream0; see DMA2_Stream0_IRQHandler() in adc.c, which sets
 * adc_ready once the full 3-channel sweep has been delivered.
 * --------------------------------------------------------------------------*/

/* ============================================================================
 * USART1_IRQHandler - Receive Bluetooth commands
 * ----------------------------------------------------------------------------
 * Still flag-only: bytes are buffered, and an idle line raises dataReady.
 * No processing is done here.
 * ==========================================================================*/
void USART1_IRQHandler(void)
{
    // RXNE: Receive data register not empty
    if (USART1->SR & (1 << 5))
    {
        char c = (char)USART1->DR;

        // Ignore newline/carriage return
        if (c == '\n' || c == '\r')
            return;

        // Add character to buffer (drops extra bytes when full)
        if (bufferIndex < sizeof(rxBuffer) - 1)
            rxBuffer[bufferIndex++] = c;
    }

    // IDLE: Idle line detected (end of transmission)
    if (USART1->SR & (1 << 4))
    {
        volatile uint32_t tmp = USART1->SR;
        tmp = USART1->DR;
        (void)tmp;  // Clear IDLE flag

        rxBuffer[bufferIndex] = '\0';  // Null-terminate
        dataReady = 1;  // Set flag
    }
}
