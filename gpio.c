/*
 * gpio.c
 * ----------------------------------------------------------------------------
 * GPIO pin configuration and external interrupt (EXTI) setup.
 * ----------------------------------------------------------------------------
 */
#include "gpio.h"

/* ============================================================================
 * GPIO_Init - Configure all GPIO pins
 * ==========================================================================*/
void GPIO_Init(void)
{
    // Enable GPIOA and GPIOB clocks
    RCC->AHB1ENR |= (1 << 0) | (1 << 1);  // GPIOA and GPIOB

    // ===== GPIOA Configuration =====

    // PA0 as input (button) with pull-down
    GPIOA->MODER &= ~(3 << 0);   // Input mode
    GPIOA->PUPDR &= ~(3 << 0);
    GPIOA->PUPDR |= (2 << 0);    // Pull-down

    // PA1, PA4, PA5 as analog (ADC inputs)
    GPIOA->MODER &= ~((3 << 2) | (3 << 8) | (3 << 10));  // Clear bits
    GPIOA->MODER |= (3 << 2) | (3 << 8) | (3 << 10);     // Analog mode

    // PA2, PA3 as alternate function (USART2)
    GPIOA->MODER &= ~((3 << 4) | (3 << 6));  // Clear bits
    GPIOA->MODER |= (2 << 4) | (2 << 6);     // Alternate function
    GPIOA->AFR[0] &= ~((0xF << 8) | (0xF << 12));  // Clear AF bits
    GPIOA->AFR[0] |= (7 << 8) | (7 << 12);         // AF7 (USART2)

    // PA6, PA7 as alternate function (TIM3_CH1, TIM3_CH2)
    GPIOA->MODER &= ~((3 << 12) | (3 << 14));  // Clear bits
    GPIOA->MODER |= (2 << 12) | (2 << 14);     // Alternate function
    GPIOA->AFR[0] &= ~((0xF << 24) | (0xF << 28));  // Clear AF bits
    GPIOA->AFR[0] |= (2 << 24) | (2 << 28);         // AF2 (TIM3)

    // ===== GPIOB Configuration =====

    // PB0, PB1 as alternate function (TIM3_CH3, TIM3_CH4)
    GPIOB->MODER &= ~((3 << 0) | (3 << 2));  // Clear bits
    GPIOB->MODER |= (2 << 0) | (2 << 2);     // Alternate function
    GPIOB->AFR[0] &= ~((0xF << 0) | (0xF << 4));  // Clear AF bits
    GPIOB->AFR[0] |= (2 << 0) | (2 << 4);         // AF2 (TIM3)

    // PB6, PB7 as alternate function (USART1 -> ESP32 link)
    GPIOB->MODER &= ~((3 << 12) | (3 << 14));  // Clear bits
    GPIOB->MODER |= (2 << 12) | (2 << 14);     // Alternate function
    GPIOB->AFR[0] &= ~((0xF << 24) | (0xF << 28));  // Clear AF bits
    GPIOB->AFR[0] |= (7 << 24) | (7 << 28);         // AF7 (USART1)

    // PB8, PB9 as alternate function (I2C1)
    GPIOB->MODER &= ~((3 << 16) | (3 << 18));  // Clear bits
    GPIOB->MODER |= (2 << 16) | (2 << 18);     // Alternate function
    GPIOB->AFR[1] &= ~((0xF << 0) | (0xF << 4));  // Clear AF bits
    GPIOB->AFR[1] |= (4 << 0) | (4 << 4);         // AF4 (I2C1)
    GPIOB->OTYPER |= (1 << 8) | (1 << 9);     // Open-drain
    GPIOB->PUPDR |= (1 << 16) | (1 << 18);    // Pull-up
}

/* ============================================================================
 * GPIO_EXTI_Config - Configure external interrupt for PA0
 * ==========================================================================*/
void GPIO_EXTI_Config(void)
{
    // Enable SYSCFG clock for EXTI
    RCC->APB2ENR |= (1 << 14);  // SYSCFGEN

    // Connect PA0 to EXTI0
    SYSCFG->EXTICR[0] &= ~(0xF << 0);  // PA0 to EXTI0

    // Configure EXTI0: Rising edge trigger
    EXTI->RTSR |= (1 << 0);   // Rising edge
    EXTI->FTSR &= ~(1 << 0);  // Not falling edge
    EXTI->IMR |= (1 << 0);    // Unmask EXTI0

    // Enable EXTI0 interrupt in NVIC
    NVIC_SetPriority(EXTI0_IRQn, 1);
    NVIC_EnableIRQ(EXTI0_IRQn);
}
