/*
 * usart.c
 * ----------------------------------------------------------------------------
 * USART2 driver for Bluetooth communication (9600 baud, PA2/PA3).
 * ----------------------------------------------------------------------------
 */
#include "usart.h"

/* ============================================================================
 * USART2_Init - Initialize USART2 with RX/IDLE interrupts
 * ==========================================================================*/
void USART2_Init(void)
{
    // Enable USART2 clock
    RCC->APB1ENR |= (1 << 17);  // USART2EN

    // Configure baud rate: 9600 @ 8 MHz
    // BRR = 8,000,000 / 9600 = 833.33 -> 833
    USART2->BRR = 833;

    // Enable USART2: TX, RX, RXNE interrupt
    USART2->CR1 |= (1 << 13);  // UE: USART enable
    USART2->CR1 |= (1 << 5);   // RXNEIE: RXNE interrupt enable
    USART2->CR1 |= (1 << 4);   // IDLE interrupt enable
    USART2->CR1 |= (1 << 3);   // TE: Transmitter enable
    USART2->CR1 |= (1 << 2);   // RE: Receiver enable

    // Enable USART2 interrupt in NVIC
    NVIC_SetPriority(USART2_IRQn, 0);  // Highest priority
    NVIC_EnableIRQ(USART2_IRQn);
}

/* ============================================================================
 * USART2_SendChar - Transmit a single character
 * ==========================================================================*/
void USART2_SendChar(char c)
{
    while (!(USART2->SR & (1 << 7)));  // Wait for TXE
    USART2->DR = c;
}

/* ============================================================================
 * USART2_SendString - Transmit a null-terminated string
 * ==========================================================================*/
void USART2_SendString(char *str)
{
    while (*str)
        USART2_SendChar(*str++);
    while (!(USART2->SR & (1 << 6)));  // Wait for TC
}

/* ============================================================================
 * USART2_SendNumber - Transmit an unsigned integer as decimal text
 * ==========================================================================*/
void USART2_SendNumber(uint16_t num)
{
    char buffer[6];
    int i = 0;

    if (num == 0)
    {
        USART2_SendChar('0');
        return;
    }

    while (num > 0)
    {
        buffer[i++] = (num % 10) + '0';
        num /= 10;
    }

    while (i > 0)
    {
        USART2_SendChar(buffer[--i]);
    }
}
