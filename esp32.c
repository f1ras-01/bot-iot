/*
 * esp32.c
 * ----------------------------------------------------------------------------
 * USART1 link to the ESP32 WiFi co-processor (PB6=TX, PB7=RX, 115200 baud).
 *
 * USART1 is on APB2. In this project APB2 is clocked from the 8 MHz HSE, so
 * the baud-rate divisor is 8 MHz / 115200.
 *
 * This driver is transmit-only for now (STM32 -> ESP32). RX is initialized so
 * the link can be made bidirectional later (e.g. ESP32 status replies).
 * ----------------------------------------------------------------------------
 */
#include "esp32.h"

/* ============================================================================
 * ESP32_Init - Initialize USART1
 * ==========================================================================*/
void ESP32_Init(void)
{
    // Enable USART1 clock (APB2)
    RCC->APB2ENR |= (1 << 4);  // USART1EN

    // Baud rate: 115200 @ 8 MHz APB2
    // BRR = 8,000,000 / 115200 = 69.44 -> 69 (error ~0.6%, well within tolerance)
    USART1->BRR = 69;

    // Enable USART1: transmitter, receiver, USART itself
    USART1->CR1 |= (1 << 13);  // UE: USART enable
    USART1->CR1 |= (1 << 3);   // TE: transmitter enable
    USART1->CR1 |= (1 << 2);   // RE: receiver enable
}

/* ============================================================================
 * ESP32_SendChar - Transmit one character
 * ==========================================================================*/
void ESP32_SendChar(char c)
{
    while (!(USART1->SR & (1 << 7)));  // Wait for TXE
    USART1->DR = (uint8_t)c;
}

/* ============================================================================
 * ESP32_SendString - Transmit a null-terminated string
 * ==========================================================================*/
void ESP32_SendString(const char *str)
{
    while (*str)
        ESP32_SendChar(*str++);
    while (!(USART1->SR & (1 << 6)));  // Wait for TC (transmission complete)
}

/* ----------------------------------------------------------------------------
 * SendUint - append an unsigned integer in decimal (helper, no printf).
 * --------------------------------------------------------------------------*/
static void SendUint(uint32_t v)
{
    char buf[11];   // enough for 32-bit
    int i = 0;

    if (v == 0)
    {
        ESP32_SendChar('0');
        return;
    }
    while (v > 0)
    {
        buf[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (i > 0)
        ESP32_SendChar(buf[--i]);
}

/* ============================================================================
 * ESP32_SelectServer - tell the ESP32 which server to publish to
 * ==========================================================================*/
void ESP32_SelectServer(esp_server_t server)
{
    switch (server)
    {
        case SERVER_THINGSPEAK:
            ESP32_SendString("#SERVER,THINGSPEAK\n");
            break;
        case SERVER_MQTT:
            ESP32_SendString("#SERVER,MQTT\n");
            break;
        case SERVER_NONE:
        default:
            ESP32_SendString("#SERVER,NONE\n");
            break;
    }
}

/* ============================================================================
 * ESP32_SendWiFiCreds - forward WiFi credentials to the ESP32
 * ----------------------------------------------------------------------------
 * The ESP32 stores these in its own flash, so credentials only need to be
 * sent once per network. ssid/password must not contain ',' or '\n'.
 * ==========================================================================*/
void ESP32_SendWiFiCreds(const char *ssid, const char *password)
{
    ESP32_SendString("#WIFI,");
    ESP32_SendString(ssid);
    ESP32_SendChar(',');
    ESP32_SendString(password);
    ESP32_SendChar('\n');
}

/* ============================================================================
 * ESP32_SendData - send one sensor frame
 * ----------------------------------------------------------------------------
 * Format: #DATA,<pot1>,<pot2>,<pot3>,<temp_x10>\n
 * temp_x10 may be negative; a leading '-' is emitted if so.
 * ==========================================================================*/
void ESP32_SendData(uint16_t pot1, uint16_t pot2, uint16_t pot3,
                    int16_t temp_x10)
{
    ESP32_SendString("#DATA,");
    SendUint(pot1);
    ESP32_SendChar(',');
    SendUint(pot2);
    ESP32_SendChar(',');
    SendUint(pot3);
    ESP32_SendChar(',');

    if (temp_x10 < 0)
    {
        ESP32_SendChar('-');
        SendUint((uint32_t)(-temp_x10));
    }
    else
    {
        SendUint((uint32_t)temp_x10);
    }

    ESP32_SendChar('\n');
}
