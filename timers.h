/*
 * timers.h
 * ----------------------------------------------------------------------------
 * Timer drivers:
 *  - TIM2 : periodic trigger source for the ADC (~333 ms).
 *  - TIM3 : 4-channel PWM generator for the motors.
 * ----------------------------------------------------------------------------
 */
#ifndef TIMERS_H
#define TIMERS_H

#include "stm32f4xx.h"

/* ----- TIM2 : ADC trigger ----- */
void TIM2_Config(void);
void TIM2_Start(void);
void TIM2_Stop(void);

/* ----- TIM3 : motor PWM ----- */
void TIM3_Init(void);

#endif /* TIMERS_H */
