/*
 * config.h
 * ----------------------------------------------------------------------------
 * Project-wide constants and hardware definitions.
 *
 * Pin Configuration:
 *  - PA0          : Button (Toggle ADC readings)
 *  - PA1          : Potentiometer 1 (ADC1_IN1)
 *  - PA2          : USART2_TX -> Bluetooth TX
 *  - PA3          : USART2_RX -> Bluetooth RX
 *  - PA4          : Potentiometer 2 (ADC1_IN4)
 *  - PA5          : Potentiometer 3 (ADC1_IN5)
 *  - PA6,PA7,PB0,PB1 : PWM for motors (TIM3)
 *  - PB8, PB9     : I2C1 for DS1621 temperature sensor
 * ----------------------------------------------------------------------------
 */
#ifndef CONFIG_H
#define CONFIG_H

#include "stm32f4xx.h"

/* ----- DS1621 temperature sensor ----- */
#define DS1621_ADDR          0x48
#define DS1621_START_CONVERT 0xEE
#define DS1621_READ_TEMP     0xAA

/* ----- Bluetooth RX buffer -----
 * Must fit the longest command. The WIFI: credential command can be
 * "WIFI:" + 32-char SSID + ";" + 63-char password + terminator. */
#define RX_BUFFER_SIZE       110

/* ----- ESP32 / cloud publishing -----
 * The STM32 runs a round-robin between the two servers:
 *   ThingSpeak active -> close -> rest -> MQTT active -> close -> rest -> ...
 * ThingSpeak free accounts accept at most one update per 15 s, so only one
 * publish is sent inside a ThingSpeak slot. */
#define SERVER_SLOT_MS       30000U   /* how long each server stays active   */
#define SERVER_REST_MS        2000U   /* idle gap between the two servers    */

#endif /* CONFIG_H */
