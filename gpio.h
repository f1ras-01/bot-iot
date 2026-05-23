/*
 * gpio.h
 * ----------------------------------------------------------------------------
 * GPIO pin configuration and external interrupt (EXTI) setup.
 * ----------------------------------------------------------------------------
 */
#ifndef GPIO_H
#define GPIO_H

#include "stm32f4xx.h"

/* Configure all GPIO pins used by the project */
void GPIO_Init(void);

/* Configure external interrupt for PA0 (button) */
void GPIO_EXTI_Config(void);

#endif /* GPIO_H */
