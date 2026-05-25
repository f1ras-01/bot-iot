/*
 * ============================================================================
 * STM32 COMPLETE BLUETOOTH ROBOT SYSTEM
 * ============================================================================
 * Features:
 *  - Motor control via Bluetooth (Forward/Backward/Left/Right)
 *  - Temperature sensing (DS1621 via I2C)
 *  - 3 Potentiometers (ADC readings on PA1, PA4, PA5)
 *  - Button control (PA0) to start/stop ADC periodic readings
 *  - Bluetooth communication (USART1, HC-06)
 *
 * Robustness notes (vs. the original single-file version):
 *  - SysTick provides a real 1 ms timebase; loop timing no longer relies on
 *    calibration-dependent busy loops.
 *  - All I2C / sensor calls have timeouts, so a missing DS1621 cannot hang
 *    the boot sequence.
 *  - ISRs only set flags; UART printing happens here in the main loop.
 *  - The ADC is fed by DMA: TIM2 triggers a 3-channel sweep and DMA2 Stream0
 *    delivers the results, raising one interrupt per sweep instead of three.
 *  - Motors run until an explicit Stop (S) command.
 *  - WiFi: the ESP32 runs ESP-AT firmware and acts as a WiFi modem. The STM32
 *    drives it with AT commands over USART2 and publishes to ThingSpeak.
 *    WiFi credentials are hardcoded in config.h. Send "STATUS" over Bluetooth
 *    to query the ESP connection state for debugging.
 *  - The independent watchdog (IWDG) resets the MCU if the loop ever stalls.
 * ============================================================================
 */

#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "system.h"
#include "gpio.h"
#include "adc.h"
#include "timers.h"
#include "i2c.h"
#include "ds1621.h"
#include "usart.h"
#include "motor.h"
#include "esp32.h"
#include "app.h"

/* Set when the motors are driving, cleared when stopped. Used so the Stop
 * command and the watchdog-style reporting only act when relevant. */
static uint8_t motors_active = 0;

/* ----------------------------------------------------------------------------
 * StartADC / StopADC - small helpers so the button handler and the
 * START/STOP text commands share one implementation.
 * --------------------------------------------------------------------------*/
static void StartADC(void)
{
    if (!adc_running)
    {
        adc_ready = 0;
        ADC_Enable();
        TIM2_Start();        // Each TIM2 trigger runs a full DMA-fed sweep
        adc_running = 1;
    }
}

static void StopADC(void)
{
    if (adc_running)
    {
        TIM2_Stop();
        adc_running = 0;
    }
}

/* ----------------------------------------------------------------------------
 * Drive - apply a motor direction.
 * Motors run until an explicit Stop command (S) is received; this matches a
 * controller that sends one-shot commands rather than a continuous stream.
 * --------------------------------------------------------------------------*/
static void Drive(int in1, int in2, int in3, int in4)
{
    SetMotorDirection(in1, in2, in3, in4);
    motors_active = 1;
}

/* ============================================================================
 * MAIN FUNCTION
 * ==========================================================================*/
int main(void)
{
    char msg[80];

    /* ----- Initialize all peripherals ----- */
    Clock_Init();            // MCU runs on the default 16 MHz HSI
    SysTick_Init();          // 1 ms timebase, needed by DelayMs/Millis
    GPIO_Init();
    GPIO_EXTI_Config();
    ADC_Config();
    TIM2_Config();
    TIM3_Init();
    I2C1_Init();
    BT_Init();
    ESP32_Init();            // USART1 link to the ESP-AT WiFi modem

    /* DS1621 may be absent; this no longer hangs the boot if it is. */
    int sensor_ok = (DS1621_Init() == DS1621_OK);

    DelayMs(50);

    /* ----- Startup banner ----- */
    BT_SendString("\r\n");
    BT_SendString("============================================\r\n");
    BT_SendString("  STM32 COMPLETE BLUETOOTH ROBOT SYSTEM    \r\n");
    BT_SendString("============================================\r\n");
    BT_SendString("HARDWARE:\r\n");
    BT_SendString("  PA0 - Button (Toggle ADC)\r\n");
    BT_SendString("  PA1,PA4,PA5 - Potentiometers 1-3\r\n");
    BT_SendString("  Motors on PA6,PA7,PB0,PB1\r\n");
    BT_SendString("  Temperature sensor on PB8,PB9\r\n\r\n");
    BT_SendString(sensor_ok ? "Temp sensor: OK\r\n"
                                : "Temp sensor: NOT DETECTED\r\n");
    BT_SendString("COMMANDS:\r\n");
    BT_SendString("  F/B/L/R/S  - Motor control\r\n");
    BT_SendString("  LF/LB/RF/RB - Individual motors\r\n");
    BT_SendString("  0-9 - Speed | ? - Show speed\r\n");
    BT_SendString("  T - Temperature\r\n");
    BT_SendString("  A - Read ADC | START/STOP - ADC control\r\n");
    BT_SendString("  H - Help\r\n");
    BT_SendString("============================================\r\n\r\n");

    sprintf(msg, "Speed: %d%%\r\n", (speed * 100) / 999);
    BT_SendString(msg);
    BT_SendString("Press PA0 button to start/stop ADC\r\n\r\n");

    /* Start the watchdog only after init/banner are done, so a slow init
     * cannot trigger a spurious reset. ~1.25 s timeout. */
    IWDG_Start();

    uint32_t last_temp_time = Millis();

    /* ----- ThingSpeak publishing state -----
     * The STM32 drives the ESP-AT module directly: connect to WiFi using the
     * credentials in config.h, then publish one sample every TS_INTERVAL_MS.
     * ThingSpeak free accounts accept at most one update per 15 s.
     * ESP connection state is reported over Bluetooth for debugging. */
    uint8_t  wifi_connected = 0;   /* AT+CWJAP succeeded                 */
    uint32_t last_publish   = Millis();

    /* Probe the ESP-AT module once at startup and report over Bluetooth. */
    if (ESP32_Ping() == ESP_OK)
        BT_SendString("ESP> module detected (AT OK)\r\n");
    else
        BT_SendString("ESP> NO RESPONSE - check wiring/baud\r\n");

    /* ----- Main loop ----- */
    while (1)
    {
        /* Kick the watchdog every iteration. If the loop ever stalls (e.g.
         * an unforeseen hang), the MCU resets instead of freezing. */
        IWDG_Refresh();

        /* ----- Deferred button event (set by EXTI0 ISR) ----- */
        if (button_event)
        {
            button_event = 0;

            if (!adc_running)
            {
                StartADC();
                BT_SendString("\r\n>>> ADC STARTED <<<\r\n");
            }
            else
            {
                StopADC();
                BT_SendString("\r\n>>> ADC STOPPED <<<\r\n");
            }

            /* Time-based debounce: ignore further edges for a short window,
             * without busy-waiting inside the ISR. */
            DelayMs(50);
            button_event = 0;   // Discard any bounce captured during the wait
        }

        /* ----- ADC results ready ----- */
        if (adc_ready && adc_running)
        {
            adc_ready = 0;
            SendADCValues();
        }

        /* ----- Periodic temperature reading (every 5 s, real time) ----- */
        if ((uint32_t)(Millis() - last_temp_time) >= 5000U)
        {
            last_temp_time = Millis();

            if (sensor_ok)
            {
                float t;
                if (DS1621_ReadTemperature(&t) == DS1621_OK)
                {
                    temperature = t;
                    /* Integer formatting: no %f dependency. */
                    int whole = (int)t;
                    int frac  = (int)((t - whole) * 10);
                    if (frac < 0) frac = -frac;
                    sprintf(msg, "AUTO> Temperature: %d.%d C\r\n", whole, frac);
                    BT_SendString(msg);
                }
                else
                {
                    BT_SendString("AUTO> Temp read failed\r\n");
                }
            }
        }

        /* ----- ThingSpeak publishing -----
         * 1. If not connected, join WiFi (AT+CWJAP) with config.h credentials.
         * 2. While connected, publish one sample every TS_INTERVAL_MS.
         * ESP connection state is reported over Bluetooth for debugging. */
        if (!wifi_connected)
        {
            BT_SendString("ESP> connecting to WiFi \"");
            BT_SendString(WIFI_SSID);
            BT_SendString("\" ...\r\n");

            if (ESP32_WiFiConnect(WIFI_SSID, WIFI_PASS) == ESP_OK)
            {
                wifi_connected = 1;
                last_publish   = Millis() - TS_INTERVAL_MS;  /* publish soon */
                BT_SendString("ESP> WiFi CONNECTED\r\n");
            }
            else
            {
                /* Failed: wait before retrying so we don't spam AT+CWJAP. */
                BT_SendString("ESP> WiFi connect FAILED, retrying in 5s\r\n");
                DelayMs(5000);
            }
        }

        if (wifi_connected &&
            (uint32_t)(Millis() - last_publish) >= TS_INTERVAL_MS)
        {
            last_publish = Millis();

            uint16_t p1 = adc_values[0];   /* snapshot (DMA-updated) */
            uint16_t p2 = adc_values[1];
            uint16_t p3 = adc_values[2];
            int16_t  temp_x10 = (int16_t)(temperature * 10.0f);

            BT_SendString("ESP> publishing to ThingSpeak...\r\n");
            if (ESP32_ThingSpeakPublish(THINGSPEAK_API_KEY,
                                        p1, p2, p3, temp_x10) == ESP_OK)
            {
                BT_SendString("ESP> ThingSpeak update SENT\r\n");
            }
            else
            {
                /* A failed publish may mean the link dropped; force a
                 * reconnect on the next loop. */
                BT_SendString("ESP> ThingSpeak publish FAILED (link lost?)\r\n");
                wifi_connected = 0;
            }
        }

        /* ----- Process a completed Bluetooth command ----- */
        if (dataReady)
        {
            /* Copy the command out under a brief interrupt mask, then clear
             * the buffer state atomically. This removes the race where a new
             * byte arrives between memset() and bufferIndex=0. */
            char cmd[RX_BUFFER_SIZE];
            __disable_irq();
            strncpy(cmd, rxBuffer, sizeof(cmd));
            cmd[sizeof(cmd) - 1] = '\0';
            bufferIndex = 0;
            dataReady = 0;
            rxBuffer[0] = '\0';
            __enable_irq();

            BT_SendString("CMD> ");
            BT_SendString(cmd);
            BT_SendString("\r\n");

            /* WiFi status query: report the ESP connection state. */
            if (strcmp(cmd, "STATUS") == 0 || strcmp(cmd, "status") == 0)
            {
                if (wifi_connected)
                    BT_SendString("ESP> state: WiFi CONNECTED, publishing\r\n");
                else
                    BT_SendString("ESP> state: NOT connected\r\n");

                if (ESP32_Ping() == ESP_OK)
                    BT_SendString("ESP> module: responding (AT OK)\r\n");
                else
                    BT_SendString("ESP> module: NO RESPONSE\r\n");
            }
            /* Motor control commands */
            else if (strcmp(cmd, "S") == 0 || strcmp(cmd, "s") == 0)
            {
                EmergencyStop();
                motors_active = 0;
            }
            else if (strcmp(cmd, "F") == 0 || strcmp(cmd, "f") == 0)
            {
                BT_SendString("Moving FORWARD\r\n");
                Drive(1, 0, 1, 0);
            }
            else if (strcmp(cmd, "B") == 0 || strcmp(cmd, "b") == 0)
            {
                BT_SendString("Moving BACKWARD\r\n");
                Drive(0, 1, 0, 1);
            }
            else if (strcmp(cmd, "L") == 0 || strcmp(cmd, "l") == 0)
            {
                BT_SendString("Turning LEFT\r\n");
                Drive(0, 0, 1, 0);
            }
            else if (strcmp(cmd, "R") == 0 || strcmp(cmd, "r") == 0)
            {
                BT_SendString("Turning RIGHT\r\n");
                Drive(1, 0, 0, 0);
            }
            else if (strcmp(cmd, "LF") == 0 || strcmp(cmd, "lf") == 0)
            {
                BT_SendString("LEFT motor FORWARD\r\n");
                Drive(1, 0, 0, 0);
            }
            else if (strcmp(cmd, "LB") == 0 || strcmp(cmd, "lb") == 0)
            {
                BT_SendString("LEFT motor BACKWARD\r\n");
                Drive(0, 1, 0, 0);
            }
            else if (strcmp(cmd, "RF") == 0 || strcmp(cmd, "rf") == 0)
            {
                BT_SendString("RIGHT motor FORWARD\r\n");
                Drive(0, 0, 1, 0);
            }
            else if (strcmp(cmd, "RB") == 0 || strcmp(cmd, "rb") == 0)
            {
                BT_SendString("RIGHT motor BACKWARD\r\n");
                Drive(0, 0, 0, 1);
            }
            /* Temperature command */
            else if (strcmp(cmd, "T") == 0 || strcmp(cmd, "t") == 0)
            {
                if (sensor_ok)
                {
                    float t;
                    if (DS1621_ReadTemperature(&t) == DS1621_OK)
                    {
                        temperature = t;
                        int whole = (int)t;
                        int frac  = (int)((t - whole) * 10);
                        if (frac < 0) frac = -frac;
                        sprintf(msg, "Temperature: %d.%d C\r\n", whole, frac);
                        BT_SendString(msg);
                    }
                    else
                    {
                        BT_SendString("Temp read failed\r\n");
                    }
                }
                else
                {
                    BT_SendString("Temp sensor not available\r\n");
                }
            }
            /* ADC commands */
            else if (strcmp(cmd, "A") == 0 || strcmp(cmd, "a") == 0)
            {
                SendADCValues();
            }
            else if (strcmp(cmd, "START") == 0)
            {
                if (!adc_running)
                {
                    StartADC();
                    BT_SendString("ADC Started\r\n");
                }
            }
            else if (strcmp(cmd, "STOP") == 0)
            {
                if (adc_running)
                {
                    StopADC();
                    BT_SendString("ADC Stopped\r\n");
                }
            }
            /* Speed control */
            else if (cmd[0] >= '0' && cmd[0] <= '9' && cmd[1] == '\0')
            {
                int speedLevel = cmd[0] - '0';
                speed = (speedLevel * 999) / 9;
                sprintf(msg, "Speed: Level %d (%d%%)\r\n",
                        speedLevel, (speed * 100) / 999);
                BT_SendString(msg);
            }
            else if (strcmp(cmd, "?") == 0)
            {
                sprintf(msg, "Speed: %d%% | ADC: %s\r\n",
                        (speed * 100) / 999,
                        adc_running ? "RUNNING" : "STOPPED");
                BT_SendString(msg);
            }
            /* Help */
            else if (strcmp(cmd, "H") == 0 || strcmp(cmd, "h") == 0)
            {
                BT_SendString("\r\nCommands:\r\n");
                BT_SendString("  Motors: F,B,L,R,S,LF,LB,RF,RB\r\n");
                BT_SendString("  Speed: 0-9, ?\r\n");
                BT_SendString("  Sensors: T, A, START, STOP\r\n");
                BT_SendString("  WiFi: STATUS (show ESP state)\r\n");
                BT_SendString("  Help: H\r\n");
            }
            else
            {
                BT_SendString("Unknown. Send 'H' for help.\r\n");
            }
        }

        /* Short, real cooperative pause. The loop period is now bounded and
         * the watchdog is refreshed every iteration above. */
        DelayMs(1);
    }
}
