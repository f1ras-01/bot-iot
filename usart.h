/*
 * usart.h
 * ----------------------------------------------------------------------------
 * Bluetooth (HC-06) driver - USART1, PA9 = TX, PA10 = RX, 9600 baud.
 *
 * Carries motor commands in and debug/status text out. The public API is
 * named BT_* (Bluetooth) so it reflects purpose, not the USART number -
 * the ESP-AT WiFi modem uses a separate UART (USART2, see esp32.h).
 * ----------------------------------------------------------------------------
 */
#ifndef USART_H
#define USART_H

#include "stm32f4xx.h"

void BT_Init(void);
void BT_SendChar(char c);
void BT_SendString(char *str);
void BT_SendNumber(uint16_t num);

#endif /* USART_H */
