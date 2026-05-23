/*
 * i2c.h
 * ----------------------------------------------------------------------------
 * Low-level I2C1 master driver (100 kHz standard mode, PB8/PB9).
 *
 * Every blocking operation has a timeout. Functions return 0 on success and
 * non-zero on failure (timeout) so callers never hang on a missing/stuck
 * slave. After a failed transaction the bus is released with a STOP.
 * ----------------------------------------------------------------------------
 */
#ifndef I2C_H
#define I2C_H

#include "stm32f4xx.h"

/* Return codes */
#define I2C_OK       0
#define I2C_TIMEOUT  1

void    I2C1_Init(void);

/* All of the following return I2C_OK or I2C_TIMEOUT. */
int     I2C1_Start(void);
int     I2C1_SendAddress(uint8_t address, uint8_t direction);
int     I2C1_Write(uint8_t data);

/* Reads one byte into *out. ack=1 to ACK, 0 to NACK. Returns I2C_OK/I2C_TIMEOUT. */
int     I2C1_Read(uint8_t ack, uint8_t *out);

void    I2C1_Stop(void);

#endif /* I2C_H */
