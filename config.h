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

/* ----- Bluetooth RX buffer ----- */
#define RX_BUFFER_SIZE       50

/* ----- Motor command failsafe -----
 * If no drive command is received within this many milliseconds, the main
 * loop stops the motors. Protects against a dropped Bluetooth link leaving
 * the robot running away. */
#define MOTOR_CMD_TIMEOUT_MS 1000U

#endif /* CONFIG_H */
