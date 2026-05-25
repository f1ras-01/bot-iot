/*
 * usart.c
 * ----------------------------------------------------------------------------
 * Bluetooth (HC-06) driver - USART1, PA9 = TX, PA10 = RX, 9600 baud.
 *
 * USART1 is on APB2 (16 MHz HSI). The received bytes are
 * handled by USART1_IRQHandler in app.c (motor commands).
 * ----------------------------------------------------------------------------
 */
#include "usart.h"

/* ============================================================================
 * BT_Init - Initialize USART1 with RX/IDLE interrupts
 * ==========================================================================*/
void BT_Init(void)
{
    // Enable USART1 clock (APB2)
    RCC->APB2ENR |= (1 << 4);  // USART1EN

    // Baud rate: 9600 @ 16 MHz APB2
    // BRR = 16,000,000 / 9600 = 1667 -> 0x683
    USART1->BRR = 1667;

    // Enable USART1: TX, RX, RXNE interrupt, IDLE interrupt
    USART1->CR1 |= (1 << 13);  // UE: USART enable
    USART1->CR1 |= (1 << 5);   // RXNEIE: RXNE interrupt enable
    USART1->CR1 |= (1 << 4);   // IDLEIE: IDLE interrupt enable
    USART1->CR1 |= (1 << 3);   // TE: Transmitter enable
    USART1->CR1 |= (1 << 2);   // RE: Receiver enable

    // Enable USART1 interrupt in NVIC
    NVIC_SetPriority(USART1_IRQn, 0);  // Highest priority
    NVIC_EnableIRQ(USART1_IRQn);
}

/* ============================================================================
 * BT_SendChar - Transmit a single character
 * ==========================================================================*/
void BT_SendChar(char c)
{
    while (!(USART1->SR & (1 << 7)));  // Wait for TXE
    USART1->DR = c;
}

/* ============================================================================
 * BT_SendString - Transmit a null-terminated string
 * ==========================================================================*/
void BT_SendString(char *str)
{
    while (*str)
        BT_SendChar(*str++);
    while (!(USART1->SR & (1 << 6)));  // Wait for TC
}

/* ============================================================================
 * BT_SendNumber - Transmit an unsigned integer as decimal text
 * ==========================================================================*/
void BT_SendNumber(uint16_t num)
{
    char buffer[6];
    int i = 0;

    if (num == 0)
    {
        BT_SendChar('0');
        return;
    }

    while (num > 0)
    {
        buffer[i++] = (num % 10) + '0';
        num /= 10;
    }

    while (i > 0)
    {
        BT_SendChar(buffer[--i]);
    }
}
