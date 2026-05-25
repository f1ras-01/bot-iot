/*
 * i2c.c
 * ----------------------------------------------------------------------------
 * Low-level I2C1 master driver (100 kHz standard mode, PB8/PB9).
 *
 * Every wait on a status flag is bounded by a loop counter. If the expected
 * flag does not appear, the function generates a STOP and returns I2C_TIMEOUT
 * instead of spinning forever. This prevents a missing or stuck DS1621 from
 * hanging the whole system.
 * ----------------------------------------------------------------------------
 */
#include "i2c.h"

/* Spin-loop iteration budget for any single flag wait. At 8 MHz this is well
 * over the time of a normal 100 kHz I2C byte, but small enough to fail fast. */
#define I2C_TIMEOUT_LOOPS  100000U

/* Wait until (expr) is true, or fail. Used inside functions returning int. */
#define I2C_WAIT(expr)                                  \
    do {                                                \
        volatile uint32_t _to = I2C_TIMEOUT_LOOPS;      \
        while (!(expr)) {                               \
            if (--_to == 0) {                           \
                I2C1_Stop();                            \
                return I2C_TIMEOUT;                     \
            }                                           \
        }                                               \
    } while (0)

/* ============================================================================
 * I2C1_Init - Initialize I2C1 peripheral
 * ==========================================================================*/
void I2C1_Init(void)
{
    RCC->APB1ENR |= (1 << 21);  // I2C1 clock enable

    // Reset I2C1
    I2C1->CR1 |= (1 << 15);     // SWRST
    I2C1->CR1 &= ~(1 << 15);    // Clear SWRST

    // Configure I2C timing for a 16 MHz APB1 clock (100 kHz standard mode).
    // FREQ = APB1 in MHz; CCR = APB1 / (2 * 100kHz); TRISE = APB1(MHz) + 1.
    I2C1->CR2   = 16;           // FREQ = 16 MHz
    I2C1->CCR   = 0x50;         // 80 -> 100 kHz standard mode
    I2C1->TRISE = 17;           // Rise time (16 + 1)

    // Enable I2C1
    I2C1->CR1 |= (1 << 0);      // PE = 1
}

/* ============================================================================
 * I2C1_Start - Generate START condition
 * ==========================================================================*/
int I2C1_Start(void)
{
    I2C1->CR1 |= (1 << 8);            // START
    I2C_WAIT(I2C1->SR1 & (1 << 0));   // Wait for SB
    return I2C_OK;
}

/* ============================================================================
 * I2C1_SendAddress - Send slave address with direction (0=write, 1=read)
 * ----------------------------------------------------------------------------
 * Also checks the AF (acknowledge failure) bit: if the slave does not ACK its
 * address, the transaction is aborted rather than waiting forever for ADDR.
 * ==========================================================================*/
int I2C1_SendAddress(uint8_t address, uint8_t direction)
{
    volatile uint32_t to = I2C_TIMEOUT_LOOPS;

    I2C1->DR = (address << 1) | direction;

    // Wait for ADDR, but bail out on a NACK (AF) or timeout.
    while (!(I2C1->SR1 & (1 << 1)))
    {
        if (I2C1->SR1 & (1 << 10))   // AF: acknowledge failure
        {
            I2C1->SR1 &= ~(1 << 10); // Clear AF
            I2C1_Stop();
            return I2C_TIMEOUT;
        }
        if (--to == 0)
        {
            I2C1_Stop();
            return I2C_TIMEOUT;
        }
    }

    (void)I2C1->SR1;  // Read SR1
    (void)I2C1->SR2;  // Read SR2 to clear ADDR
    return I2C_OK;
}

/* ============================================================================
 * I2C1_Write - Transmit one byte
 * ==========================================================================*/
int I2C1_Write(uint8_t data)
{
    I2C_WAIT(I2C1->SR1 & (1 << 7));   // Wait for TXE
    I2C1->DR = data;
    I2C_WAIT(I2C1->SR1 & (1 << 2));   // Wait for BTF
    return I2C_OK;
}

/* ============================================================================
 * I2C1_Read - Receive one byte into *out (ack=1 to ACK, 0 to NACK)
 * ----------------------------------------------------------------------------
 * On timeout *out is left unchanged and I2C_TIMEOUT is returned, so the caller
 * can no longer silently consume garbage.
 * ==========================================================================*/
int I2C1_Read(uint8_t ack, uint8_t *out)
{
    if (ack)
        I2C1->CR1 |= (1 << 10);   // ACK = 1
    else
        I2C1->CR1 &= ~(1 << 10);  // ACK = 0

    I2C_WAIT(I2C1->SR1 & (1 << 6));  // Wait for RXNE
    *out = (uint8_t)I2C1->DR;
    return I2C_OK;
}

/* ============================================================================
 * I2C1_Stop - Generate STOP condition
 * ==========================================================================*/
void I2C1_Stop(void)
{
    I2C1->CR1 |= (1 << 9);  // STOP
}
