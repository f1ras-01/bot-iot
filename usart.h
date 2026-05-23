/*
 * usart.h
 * ----------------------------------------------------------------------------
 * USART2 driver for Bluetooth communication (9600 baud, PA2/PA3).
 * ----------------------------------------------------------------------------
 */
#ifndef USART_H
#define USART_H

#include "stm32f4xx.h"

void USART2_Init(void);
void USART2_SendChar(char c);
void USART2_SendString(char *str);
void USART2_SendNumber(uint16_t num);

#endif /* USART_H */
