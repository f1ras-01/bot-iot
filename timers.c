/*
 * timers.c
 * ----------------------------------------------------------------------------
 * Timer drivers:
 *  - TIM2 : periodic trigger source for the ADC (~333 ms).
 *  - TIM3 : 4-channel PWM generator for the motors.
 * ----------------------------------------------------------------------------
 */
#include "timers.h"

/* ============================================================================
 * TIM2_Config - Configure TIM2 to trigger ADC every ~333ms
 * ==========================================================================*/
void TIM2_Config(void)
{
    // Enable TIM2 clock
    RCC->APB1ENR |= (1 << 0);  // TIM2EN

    // Reset TIM2 configuration
    TIM2->CR1 = 0;
    TIM2->CR2 = 0;

    // Configure prescaler and auto-reload for a 16 MHz timer clock
    // Timer frequency = 16 MHz / (PSC + 1) = 16 MHz / 16000 = 1 kHz
    TIM2->PSC = 15999;  // Prescaler: 16000-1

    // Auto-reload value: 1 kHz / 333 = ~3 Hz (333ms per trigger)
    TIM2->ARR = 332;  // 333ms period

    // Configure TRGO: Update event triggers ADC
    TIM2->CR2 &= ~(7 << 4);  // Clear MMS bits
    TIM2->CR2 |= (2 << 4);   // MMS = 010 (Update event as TRGO)

    // Enable update event
    TIM2->CR1 &= ~(1 << 1);  // UDIS = 0

    // Generate update event to load registers
    TIM2->EGR |= (1 << 0);   // UG = 1

    // Clear update flag
    TIM2->SR &= ~(1 << 0);   // Clear UIF
}

/* ============================================================================
 * TIM2_Start - Start TIM2 counter
 * ==========================================================================*/
void TIM2_Start(void)
{
    TIM2->CNT = 0;           // Reset counter
    TIM2->CR1 |= (1 << 0);   // CEN = 1 (start timer)
}

/* ============================================================================
 * TIM2_Stop - Stop TIM2 counter
 * ==========================================================================*/
void TIM2_Stop(void)
{
    TIM2->CR1 &= ~(1 << 0);  // CEN = 0 (stop timer)
}

/* ============================================================================
 * TIM3_Init - PWM for Motors
 * ==========================================================================*/
void TIM3_Init(void)
{
    // Enable TIM3 clock
    RCC->APB1ENR |= (1 << 1);  // TIM3EN

    // Reset TIM3 configuration
    TIM3->CR1 = 0;
    TIM3->CR2 = 0;

    // Configure prescaler and auto-reload for a 16 MHz timer clock
    // PWM frequency = 16 MHz / ((PSC+1) * (ARR+1)) = 16 MHz / (40 * 1000) = 400 Hz
    TIM3->PSC = 39;   // Prescaler: 40-1
    TIM3->ARR = 999;  // Auto-reload: 1000-1

    // Configure PWM mode for all 4 channels
    TIM3->CCMR1 |= (6 << 4) | (6 << 12);    // CH1, CH2: PWM mode 1
    TIM3->CCMR2 |= (6 << 4) | (6 << 12);    // CH3, CH4: PWM mode 1
    TIM3->CCMR1 |= (1 << 3) | (1 << 11);    // CH1, CH2: Preload enable
    TIM3->CCMR2 |= (1 << 3) | (1 << 11);    // CH3, CH4: Preload enable

    // Enable all 4 channels
    TIM3->CCER |= (1 << 0) | (1 << 4) | (1 << 8) | (1 << 12);

    // Initialize duty cycles to 0
    TIM3->CCR1 = 0;
    TIM3->CCR2 = 0;
    TIM3->CCR3 = 0;
    TIM3->CCR4 = 0;

    // Start TIM3
    TIM3->CR1 |= (1 << 0);  // CEN = 1
}
