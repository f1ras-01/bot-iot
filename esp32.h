/*
 * esp32.h
 * ----------------------------------------------------------------------------
 * Driver for an ESP32 running ESP-AT firmware, used as a WiFi modem.
 *
 * The ESP32 is NOT programmed by us - it runs the vendor ESP-AT firmware and
 * is controlled entirely by AT command strings over UART. The STM32 sends AT
 * commands and parses the responses ("OK", "ERROR", ">", "SEND OK", ...).
 * All WiFi / HTTP logic therefore lives here on the STM32.
 *
 * Link: USART2, PA2 = TX, PA3 = RX, 115200 baud, 8N1.
 *   STM32 PA2 (USART2_TX) -> ESP32 U0RXD (GPIO3)
 *   STM32 PA3 (USART2_RX) <- ESP32 U0TXD (GPIO1)
 *   Common ground mandatory. The ESP32's USB-data must be disconnected while
 *   the STM32 drives UART0, or the two TX drivers fight.
 *
 * RX is interrupt-driven into a ring buffer; the AT engine scans that buffer
 * for expected reply tokens with per-command timeouts, so a missing or stuck
 * module cannot hang the system.
 * ----------------------------------------------------------------------------
 */
#ifndef ESP32_H
#define ESP32_H

#include "stm32f4xx.h"

/* Return codes for the AT operations below. */
#define ESP_OK     0
#define ESP_FAIL   1

/* ----- Low-level link ----- */

/* Initialize USART1 (and its RX interrupt) for the ESP-AT link. */
void ESP32_Init(void);

/* ----- AT-level helpers ----- */

/* Send "AT" and check the module answers "OK". Use this as a liveness probe. */
int ESP32_Ping(void);

/* Join a WiFi access point (AT+CWMODE + AT+CWJAP). Blocks until joined or
 * timeout. Returns ESP_OK / ESP_FAIL. */
int ESP32_WiFiConnect(const char *ssid, const char *password);

/* Disconnect from WiFi (AT+CWQAP). */
void ESP32_WiFiDisconnect(void);

/* ----- ThingSpeak ----- */

/* Publish one sample to ThingSpeak over HTTP. WiFi must already be connected.
 * temp_x10 is temperature in tenths of a degree C (234 = 23.4 C).
 * Returns ESP_OK / ESP_FAIL. */
int ESP32_ThingSpeakPublish(const char *api_key,
                            uint16_t pot1, uint16_t pot2, uint16_t pot3,
                            int16_t temp_x10);

#endif /* ESP32_H */
