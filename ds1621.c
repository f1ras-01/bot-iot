/*
 * ds1621.c
 * ----------------------------------------------------------------------------
 * DS1621 digital temperature sensor driver (over I2C1).
 *
 * Each step checks the I2C return code and aborts on the first failure, so a
 * disconnected or unpowered sensor causes a clean error return instead of a
 * permanent hang. Temperature is decoded as signed two's complement.
 * ----------------------------------------------------------------------------
 */
#include "ds1621.h"
#include "config.h"
#include "i2c.h"
#include "system.h"

/* ============================================================================
 * DS1621_Init - Start continuous temperature conversion
 * ==========================================================================*/
int DS1621_Init(void)
{
    if (I2C1_Start() != I2C_OK)                       return DS1621_ERROR;
    if (I2C1_SendAddress(DS1621_ADDR, 0) != I2C_OK)   return DS1621_ERROR;
    if (I2C1_Write(DS1621_START_CONVERT) != I2C_OK)   return DS1621_ERROR;
    I2C1_Stop();
    DelayMs(10);
    return DS1621_OK;
}

/* ============================================================================
 * DS1621_ReadTemperature - Read temperature into *temp_c
 * ----------------------------------------------------------------------------
 * The MSB is the integer part in two's complement; bit 7 of the LSB is the
 * 0.5 C fraction. Casting the MSB to int8_t makes sub-zero readings correct.
 * ==========================================================================*/
int DS1621_ReadTemperature(float *temp_c)
{
    uint8_t temp_msb, temp_lsb;

    /* Issue the "read temperature" command. */
    if (I2C1_Start() != I2C_OK)                     return DS1621_ERROR;
    if (I2C1_SendAddress(DS1621_ADDR, 0) != I2C_OK) return DS1621_ERROR;
    if (I2C1_Write(DS1621_READ_TEMP) != I2C_OK)     return DS1621_ERROR;
    I2C1_Stop();

    DelayMs(1);

    /* Read the two data bytes. */
    if (I2C1_Start() != I2C_OK)                     return DS1621_ERROR;
    if (I2C1_SendAddress(DS1621_ADDR, 1) != I2C_OK) return DS1621_ERROR;
    if (I2C1_Read(1, &temp_msb) != I2C_OK)          return DS1621_ERROR;
    if (I2C1_Read(0, &temp_lsb) != I2C_OK)          return DS1621_ERROR;
    I2C1_Stop();

    /* Decode: signed integer part + optional 0.5 C. */
    float temp = (float)(int8_t)temp_msb;
    if (temp_lsb & 0x80)
        temp += 0.5f;

    *temp_c = temp;
    return DS1621_OK;
}
