/*
 * ds1621.h
 * ----------------------------------------------------------------------------
 * DS1621 digital temperature sensor driver (over I2C1).
 *
 * The driver tolerates a missing/faulty sensor: every routine reports success
 * or failure rather than hanging, so the system still boots without a sensor.
 * ----------------------------------------------------------------------------
 */
#ifndef DS1621_H
#define DS1621_H

#include "stm32f4xx.h"

/* Return codes */
#define DS1621_OK     0
#define DS1621_ERROR  1

/* Start continuous conversion. Returns DS1621_OK or DS1621_ERROR. */
int DS1621_Init(void);

/* Read temperature into *temp_c (degrees Celsius, may be negative).
 * Returns DS1621_OK on success; on error *temp_c is left unchanged. */
int DS1621_ReadTemperature(float *temp_c);

#endif /* DS1621_H */
