/*
 * adc.c
 * ----------------------------------------------------------------------------
 * ADC1 driver - 3-channel scan mode, TIM2-triggered, DMA-fed.
 * Channels: CH1 (PA1), CH4 (PA4), CH5 (PA5).
 *
 * Data path:
 *   TIM2 TRGO --> ADC1 converts CH1,CH4,CH5 in scan order
 *             --> each result raises a DMA request
 *             --> DMA2 Stream0 writes it into adc_values[0..2]
 *             --> after the 3rd transfer, DMA transfer-complete IRQ fires
 *                 and sets adc_ready.
 *
 * Compared with the previous per-channel EOC-interrupt design this cuts the
 * interrupt count from 3 per sweep to 1, and removes the channel_index
 * bookkeeping (DMA places each value in the correct slot by itself).
 *
 * ADC1 maps to DMA2, Stream 0, Channel 0 on the STM32F4.
 * ----------------------------------------------------------------------------
 */
#include "adc.h"
#include "system.h"
#include "app.h"      /* adc_values[], adc_ready */

/* ============================================================================
 * ADC_DMA_Config - Configure DMA2 Stream0 for ADC1
 * ==========================================================================*/
static void ADC_DMA_Config(void)
{
    // Enable DMA2 clock
    RCC->AHB1ENR |= (1 << 22);  // DMA2EN

    // Disable the stream before (re)configuring it, and wait until it is off
    DMA2_Stream0->CR &= ~(1 << 0);          // EN = 0
    while (DMA2_Stream0->CR & (1 << 0));    // Wait for EN to clear

    // Clear any pending interrupt flags for Stream0 (LIFCR low-stream flags)
    DMA2->LIFCR = (1 << 0) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5);

    // Channel selection: Channel 0 (ADC1) -> CHSEL = 000
    DMA2_Stream0->CR &= ~(7 << 25);

    // Addresses
    DMA2_Stream0->PAR  = (uint32_t)&ADC1->DR;        // Peripheral: ADC data reg
    DMA2_Stream0->M0AR = (uint32_t)&adc_values[0];   // Memory: result array
    DMA2_Stream0->NDTR = 3;                          // 3 items per sweep

    // Transfer direction: peripheral-to-memory (DIR = 00)
    DMA2_Stream0->CR &= ~(3 << 6);

    // Data sizes: peripheral 16-bit, memory 16-bit
    DMA2_Stream0->CR &= ~(3 << 11);   // Clear PSIZE
    DMA2_Stream0->CR |=  (1 << 11);   // PSIZE = 01 (16-bit)
    DMA2_Stream0->CR &= ~(3 << 13);   // Clear MSIZE
    DMA2_Stream0->CR |=  (1 << 13);   // MSIZE = 01 (16-bit)

    // Increment memory after each transfer; do NOT increment peripheral
    DMA2_Stream0->CR |=  (1 << 10);   // MINC = 1
    DMA2_Stream0->CR &= ~(1 << 9);    // PINC = 0

    // Circular mode: NDTR auto-reloads, so each new TIM2 sweep refills the
    // array without any reconfiguration.
    DMA2_Stream0->CR |=  (1 << 8);    // CIRC = 1

    // Transfer-complete interrupt
    DMA2_Stream0->CR |=  (1 << 4);    // TCIE = 1

    // Enable the stream
    DMA2_Stream0->CR |=  (1 << 0);    // EN = 1

    // Enable the DMA2 Stream0 interrupt in the NVIC
    NVIC_SetPriority(DMA2_Stream0_IRQn, 2);
    NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

/* ============================================================================
 * ADC_Config - Configure ADC1 in scan mode with DMA
 * ==========================================================================*/
void ADC_Config(void)
{
    // Enable ADC1 clock
    RCC->APB2ENR |= (1 << 8);  // ADC1 clock

    // Reset ADC control registers
    ADC1->CR1 = 0;
    ADC1->CR2 = 0;

    // Configure ADC1 CR1
    ADC1->CR1 |= (1 << 8);    // SCAN: scan mode enable (convert whole sequence)
    // NOTE: EOC interrupt (EOCIE) and discontinuous mode (DISCEN) are
    // intentionally NOT set - the DMA handles result transfer, and the whole
    // 3-channel sequence runs on each trigger.

    // Configure ADC1 CR2
    ADC1->CR2 &= ~(1 << 1);   // CONT = 0 (sequence runs once per trigger)
    ADC1->CR2 |= (1 << 8);    // DMA = 1  (generate a DMA request per conversion)
    ADC1->CR2 |= (1 << 9);    // DDS = 1  (keep issuing DMA requests; needed for
                              //           continuous/circular operation)
    ADC1->CR2 |= (1 << 10);   // EOCS = 1 (EOC set after each conversion)

    // External trigger: TIM2 TRGO
    ADC1->CR2 &= ~(0xF << 24);  // Clear EXTSEL
    ADC1->CR2 |= (6 << 24);     // EXTSEL = 0110 (TIM2 TRGO)
    ADC1->CR2 &= ~(3 << 28);    // Clear EXTEN
    ADC1->CR2 |= (1 << 28);     // EXTEN = 01 (rising edge)

    // Configure sequence: 3 conversions
    ADC1->SQR1 &= ~(0xF << 20);  // Clear L bits
    ADC1->SQR1 |= (2 << 20);     // L = 2 (3 conversions)

    // Set conversion sequence: CH1 (PA1), CH4 (PA4), CH5 (PA5)
    ADC1->SQR3 = 0;
    ADC1->SQR3 |= (1 << 0);   // 1st conversion: CH1
    ADC1->SQR3 |= (4 << 5);   // 2nd conversion: CH4
    ADC1->SQR3 |= (5 << 10);  // 3rd conversion: CH5

    // Sample time: 480 cycles (for stable readings)
    ADC1->SMPR2 = 0;
    ADC1->SMPR2 |= (7 << 3);   // CH1: 480 cycles
    ADC1->SMPR2 |= (7 << 12);  // CH4: 480 cycles
    ADC1->SMPR2 |= (7 << 15);  // CH5: 480 cycles

    // Configure the DMA stream that will carry the results
    ADC_DMA_Config();
}

/* ============================================================================
 * ADC_Enable - Power on ADC
 * ==========================================================================*/
void ADC_Enable(void)
{
    ADC1->CR2 |= (1 << 0);  // ADON: ADC ON
    DelayMs(1);             // Stabilization delay
}

/* ============================================================================
 * DMA2_Stream0_IRQHandler - one full 3-channel sweep has been delivered
 * ----------------------------------------------------------------------------
 * Replaces the old per-channel ADC_IRQHandler. Just clears the flag and
 * signals the main loop; adc_values[] is already populated by the DMA.
 * ==========================================================================*/
void DMA2_Stream0_IRQHandler(void)
{
    // TCIF0: transfer-complete flag for Stream0 (bit 5 of LISR)
    if (DMA2->LISR & (1 << 5))
    {
        DMA2->LIFCR = (1 << 5);  // Clear TCIF0
        adc_ready = 1;           // Signal main loop: all 3 values ready
    }
}
