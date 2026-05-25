/*
 * esp32.c
 * ----------------------------------------------------------------------------
 * Driver for an ESP32 running ESP-AT firmware (WiFi modem over UART).
 *
 * USART2, PA2=TX, PA3=RX, 115200 baud. RX is interrupt-driven into a ring
 * buffer. The AT engine sends a command string and then scans the incoming
 * bytes for an expected token (e.g. "OK"), bounded by a SysTick timeout.
 *
 * ESP-AT command flow for one ThingSpeak update:
 *   AT                                          -> OK
 *   AT+CWMODE=1                                 -> OK
 *   AT+CWJAP="ssid","pass"                      -> OK   (slow: up to ~20 s)
 *   AT+CIPSTART="TCP","api.thingspeak.com",80   -> OK
 *   AT+CIPSEND=<len>                            -> ">"
 *   <HTTP request bytes>                        -> SEND OK
 *   AT+CIPCLOSE                                 -> OK
 * ----------------------------------------------------------------------------
 */
#include "esp32.h"
#include "system.h"     /* Millis() for timeouts */
#include <string.h>
#include <stdio.h>

/* ----- RX ring buffer (filled by USART2_IRQHandler) ----- */
#define RX_RING_SIZE 512
static volatile char     rxRing[RX_RING_SIZE];
static volatile uint16_t rxHead = 0;   /* written by ISR  */
static volatile uint16_t rxTail = 0;   /* read by main    */

/* ThingSpeak endpoint. Plain HTTP keeps the AT sequence simple. */
#define TS_HOST "api.thingspeak.com"
#define TS_PORT 80

/* ============================================================================
 * ESP32_Init - Initialize USART1 with an RX interrupt
 * ==========================================================================*/
void ESP32_Init(void)
{
    RCC->APB1ENR |= (1 << 17);  // USART2EN (APB1)

    // 115200 @ 16 MHz APB1: BRR = 16e6 / 115200 = 139 -> 0x8B
    USART2->BRR = 139;

    USART2->CR1 |= (1 << 13);   // UE: USART enable
    USART2->CR1 |= (1 << 5);    // RXNEIE: RX-not-empty interrupt enable
    USART2->CR1 |= (1 << 3);    // TE: transmitter enable
    USART2->CR1 |= (1 << 2);    // RE: receiver enable

    NVIC_SetPriority(USART2_IRQn, 1);
    NVIC_EnableIRQ(USART2_IRQn);
}

/* ============================================================================
 * USART2_IRQHandler - store each received byte into the ring buffer
 * ==========================================================================*/
void USART2_IRQHandler(void)
{
    if (USART2->SR & (1 << 5))   // RXNE
    {
        char c = (char)USART2->DR;        // reading DR clears RXNE
        uint16_t next = (rxHead + 1) % RX_RING_SIZE;
        if (next != rxTail)               // drop byte if buffer full
        {
            rxRing[rxHead] = c;
            rxHead = next;
        }
    }
}

/* ----------------------------------------------------------------------------
 * rxClear - discard anything currently in the ring buffer.
 * --------------------------------------------------------------------------*/
static void rxClear(void)
{
    rxTail = rxHead;
}

/* ----------------------------------------------------------------------------
 * Low-level transmit helpers.
 * --------------------------------------------------------------------------*/
static void txChar(char c)
{
    while (!(USART2->SR & (1 << 7)));   // wait TXE
    USART2->DR = (uint8_t)c;
}

static void txString(const char *s)
{
    while (*s)
        txChar(*s++);
}

/* ----------------------------------------------------------------------------
 * WaitFor - scan incoming bytes until `token` is seen, or `fail` is seen,
 * or `timeout_ms` elapses.
 *   returns ESP_OK   if token found
 *   returns ESP_FAIL if the fail string or a timeout occurs
 * A small sliding window holds the most recent bytes for substring matching.
 * --------------------------------------------------------------------------*/
static int WaitFor(const char *token, const char *fail, uint32_t timeout_ms)
{
    char    window[24];
    uint8_t wlen = 0;
    uint32_t start = Millis();

    size_t flen = (fail != NULL) ? strlen(fail) : 0;

    while ((uint32_t)(Millis() - start) < timeout_ms)
    {
        /* AT+CWJAP can take ~20 s, far longer than the IWDG period. Refresh
         * the watchdog here so a long-but-healthy AT operation is not
         * mistaken for a hang. */
        IWDG_Refresh();

        // Pull all currently-available bytes from the ring.
        while (rxTail != rxHead)
        {
            char c = rxRing[rxTail];
            rxTail = (rxTail + 1) % RX_RING_SIZE;

            // Append to the sliding window (shift out the oldest if full).
            if (wlen < sizeof(window) - 1)
            {
                window[wlen++] = c;
            }
            else
            {
                memmove(window, window + 1, sizeof(window) - 2);
                window[sizeof(window) - 2] = c;
            }
            window[wlen] = '\0';

            if (strstr(window, token) != NULL)
                return ESP_OK;
            if (flen && strstr(window, fail) != NULL)
                return ESP_FAIL;
        }
    }
    return ESP_FAIL;   // timeout
}

/* ----------------------------------------------------------------------------
 * SendCommand - send "<cmd>\r\n" then wait for the expected reply.
 * --------------------------------------------------------------------------*/
static int SendCommand(const char *cmd, const char *token, uint32_t timeout_ms)
{
    rxClear();
    txString(cmd);
    txString("\r\n");
    return WaitFor(token, "ERROR", timeout_ms);
}

/* ============================================================================
 * ESP32_Ping - "AT" -> "OK"
 * ==========================================================================*/
int ESP32_Ping(void)
{
    return SendCommand("AT", "OK", 1000);
}

/* ============================================================================
 * ESP32_WiFiConnect - station mode + join an access point
 * ==========================================================================*/
int ESP32_WiFiConnect(const char *ssid, const char *password)
{
    char cmd[128];

    /* Reset the module first to clear any stale connection state, then give
     * the ESP-AT firmware time to reboot before the next command. */
    SendCommand("AT+RST", "ready", 5000);
    DelayMs(2000);

    if (SendCommand("AT", "OK", 1000) != ESP_OK)
        return ESP_FAIL;

    if (SendCommand("AT+CWMODE=1", "OK", 2000) != ESP_OK)
        return ESP_FAIL;

    // AT+CWJAP="ssid","password"  - joining can take many seconds.
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    if (SendCommand(cmd, "OK", 20000) != ESP_OK)
        return ESP_FAIL;

    return ESP_OK;
}

/* ============================================================================
 * ESP32_WiFiDisconnect - leave the access point
 * ==========================================================================*/
void ESP32_WiFiDisconnect(void)
{
    SendCommand("AT+CWQAP", "OK", 2000);
}

/* ============================================================================
 * ESP32_ThingSpeakPublish - open TCP, send an HTTP GET, close
 * ----------------------------------------------------------------------------
 * temp_x10 is tenths of a degree; it is formatted as a decimal with integer
 * math so no floating-point printf is needed.
 * ==========================================================================*/
int ESP32_ThingSpeakPublish(const char *api_key,
                            uint16_t pot1, uint16_t pot2, uint16_t pot3,
                            int16_t temp_x10)
{
    char cmd[96];
    char http[256];
    char tempStr[12];
    int  len;

    /* Format temperature: sign + whole.frac */
    int16_t t = temp_x10;
    const char *sign = "";
    if (t < 0) { sign = "-"; t = (int16_t)(-t); }
    snprintf(tempStr, sizeof(tempStr), "%s%d.%d", sign, t / 10, t % 10);

    /* Open a TCP connection to ThingSpeak. */
    snprintf(cmd, sizeof(cmd),
             "AT+CIPSTART=\"TCP\",\"%s\",%d", TS_HOST, TS_PORT);
    if (SendCommand(cmd, "OK", 8000) != ESP_OK)
        return ESP_FAIL;

    /* Build the HTTP GET request. ThingSpeak fields: 1..3 = pots, 4 = temp. */
    len = snprintf(http, sizeof(http),
        "GET /update?api_key=%s&field1=%u&field2=%u&field3=%u&field4=%s "
        "HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
        api_key, pot1, pot2, pot3, tempStr, TS_HOST);

    /* Tell ESP-AT how many bytes follow, then wait for the '>' prompt. */
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", len);
    if (SendCommand(cmd, ">", 4000) != ESP_OK)
    {
        SendCommand("AT+CIPCLOSE", "OK", 2000);
        return ESP_FAIL;
    }

    /* Send the raw HTTP request; ESP-AT replies "SEND OK" once transmitted. */
    rxClear();
    txString(http);
    if (WaitFor("SEND OK", "ERROR", 6000) != ESP_OK)
    {
        SendCommand("AT+CIPCLOSE", "OK", 2000);
        return ESP_FAIL;
    }

    /* Give the server a moment to reply, then close. We do not parse the
     * HTTP response body; reaching SEND OK means the request went out. */
    DelayMs(1500);
    SendCommand("AT+CIPCLOSE", "OK", 2000);

    return ESP_OK;
}
