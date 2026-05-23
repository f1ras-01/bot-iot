/*
 * motor.h
 * ----------------------------------------------------------------------------
 * Motor control - sets direction/speed via TIM3 PWM channels.
 * ----------------------------------------------------------------------------
 */
#ifndef MOTOR_H
#define MOTOR_H

#include "stm32f4xx.h"

/* Set the 4 motor PWM channels. Each in* is 0 (off) or 1 (drive at `speed`). */
void SetMotorDirection(int in1, int in2, int in3, int in4);

/* Immediately stop all motors and report over UART */
void EmergencyStop(void);

#endif /* MOTOR_H */
