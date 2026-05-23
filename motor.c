/*
 * motor.c
 * ----------------------------------------------------------------------------
 * Motor control - sets direction/speed via TIM3 PWM channels.
 * ----------------------------------------------------------------------------
 */
#include "motor.h"
#include "app.h"
#include "usart.h"

/* ============================================================================
 * SetMotorDirection - Drive the 4 PWM channels according to in1..in4
 * ==========================================================================*/
void SetMotorDirection(int in1, int in2, int in3, int in4)
{
    TIM3->CCR1 = (speed * in1);
    TIM3->CCR2 = (speed * in2);
    TIM3->CCR3 = (speed * in3);
    TIM3->CCR4 = (speed * in4);
}

/* ============================================================================
 * EmergencyStop - Stop all motors immediately
 * ==========================================================================*/
void EmergencyStop(void)
{
    TIM3->CCR1 = 0;
    TIM3->CCR2 = 0;
    TIM3->CCR3 = 0;
    TIM3->CCR4 = 0;
    USART2_SendString("\r\n>>> EMERGENCY STOP <<<\r\n");
}
