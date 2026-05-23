/*
 * adc.c
 * ----------------------------------------------------------------------------
 * ADC1 driver - 3-channel discontinuous mode triggered by TIM2.
 * Channels: CH1 (PA1), CH4 (PA4), CH5 (PA5).
 * ----------------------------------------------------------------------------
 */
#include "adc.h"
#include "system.h"

/* ============================================================================
 * ADC_Config - Configure ADC in discontinuous mode
 * ==========================================================================*/
void ADC_Config(void)
{
    // Enable ADC1 clock
    RCC->APB2ENR |= (1 << 8);  // ADC1 clock

    // Reset ADC control registers
    ADC1->CR1 = 0;
    ADC1->CR2 = 0;

    // Configure ADC1 CR1
    ADC1->CR1 |= (1 << 8);   // SCAN: Scan mode enable
    ADC1->CR1 |= (1 << 5);   // EOCIE: EOC interrupt enable
    ADC1->CR1 |= (1 << 11);  // DISCEN: Discontinuous mode enable
    ADC1->CR1 &= ~(7 << 13);  // DISCNUM = 000 (1 conversion per trigger)

    // Configure ADC1 CR2
    ADC1->CR2 &= ~(1 << 1);   // CONT = 0 (single conversion mode)
    ADC1->CR2 |= (1 << 10);   // EOCS = 1 (EOC after each conversion)

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

    // Enable ADC interrupt in NVIC
    NVIC_SetPriority(ADC_IRQn, 2);
    NVIC_EnableIRQ(ADC_IRQn);
}

/* ============================================================================
 * ADC_Enable - Power on ADC
 * ==========================================================================*/
void ADC_Enable(void)
{
    ADC1->CR2 |= (1 << 0);  // ADON: ADC ON
    DelayMs(1);             // Stabilization delay
}
