/*
 * adc.h
 * ----------------------------------------------------------------------------
 * ADC1 driver - 3-channel scan mode, TIM2-triggered, DMA-fed.
 * Channels: CH1 (PA1), CH4 (PA4), CH5 (PA5).
 *
 * On every TIM2 TRGO the ADC converts all 3 channels in scan order. DMA2
 * Stream0 copies each result straight into the shared adc_values[] array, so
 * the CPU is interrupted only once per full sweep (DMA transfer-complete)
 * instead of once per channel.
 * ----------------------------------------------------------------------------
 */
#ifndef ADC_H
#define ADC_H

#include "stm32f4xx.h"

/* Configure ADC1 (scan mode, TIM2 TRGO trigger, DMA request enabled)
 * and DMA2 Stream0 to deliver the 3 results into adc_values[]. */
void ADC_Config(void);

/* Power on the ADC */
void ADC_Enable(void);

#endif /* ADC_H */
