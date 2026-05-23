/*
 * adc.h
 * ----------------------------------------------------------------------------
 * ADC1 driver - 3-channel discontinuous mode triggered by TIM2.
 * Channels: CH1 (PA1), CH4 (PA4), CH5 (PA5).
 * ----------------------------------------------------------------------------
 */
#ifndef ADC_H
#define ADC_H

#include "stm32f4xx.h"

/* Configure ADC1 in scan + discontinuous mode, TIM2 TRGO triggered */
void ADC_Config(void);

/* Power on the ADC */
void ADC_Enable(void);

#endif /* ADC_H */
