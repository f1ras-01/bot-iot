/*
 * esp32.h
 * ----------------------------------------------------------------------------
 * USART1 link to the ESP32 WiFi co-processor (PB6=TX, PB7=RX, 115200 baud).
 *
 * The STM32 owns the sensors and orchestrates everything; the ESP32 is a
 * transport that forwards data to a cloud server (ThingSpeak) or a local
 * broker (MQTT). The STM32 runs a round-robin between the two servers and
 * tells the ESP32 what to do. The STM32 never talks to WiFi directly.
 *
 * Protocol (STM32 -> ESP32), one ASCII line per message, '\n' terminated:
 *   #SERVER,THINGSPEAK\n                      - make ThingSpeak active
 *   #SERVER,MQTT\n                            - make the MQTT broker active
 *   #SERVER,NONE\n                            - close/idle the active server
 *   #DATA,<pot1>,<pot2>,<pot3>,<temp_x10>\n   - publish one sensor sample
 *   #WIFI,<ssid>,<password>\n                 - set/store WiFi credentials
 *
 * Temperature is sent as an integer x10 (e.g. 234 = 23.4 C) so the STM32
 * never needs floating-point printf.
 * ----------------------------------------------------------------------------
 */
#ifndef ESP32_H
#define ESP32_H

#include "stm32f4xx.h"

/* Server selection passed to ESP32_SelectServer(). */
typedef enum {
    SERVER_THINGSPEAK = 0,
    SERVER_MQTT       = 1,
    SERVER_NONE       = 2   /* "rest" phase: close/idle the active server */
} esp_server_t;

/* Initialize USART1 for the ESP32 link. */
void ESP32_Init(void);

/* Send one raw character / string over the link. */
void ESP32_SendChar(char c);
void ESP32_SendString(const char *str);

/* Tell the ESP32 which server to publish to (or NONE to idle). */
void ESP32_SelectServer(esp_server_t server);

/* Forward WiFi credentials to the ESP32 (it stores them in flash).
 * ssid and password must not contain ',' or '\n'. */
void ESP32_SendWiFiCreds(const char *ssid, const char *password);

/* Send one sensor frame: 3 ADC values + temperature (in tenths of a degree).
 * Only meaningful while a server is active. */
void ESP32_SendData(uint16_t pot1, uint16_t pot2, uint16_t pot3,
                    int16_t temp_x10);

#endif /* ESP32_H */
