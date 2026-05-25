/*
 * system.c
 * ----------------------------------------------------------------------------
 * System clock, SysTick millisecond timebase, watchdog, and delays.
 *
 * The MCU runs on its default internal 16 MHz HSI oscillator - no clock
 * switching is performed (matching the proven hardware setup). Every clock-
 * derived value in the project (SysTick, UART baud, timers, I2C) is sized
 * for a 16 MHz core / APB clock.
 * ----------------------------------------------------------------------------
 */
#include "system.h"

/* Core / APB clock: the STM32F407 powers up on the 16 MHz HSI and we keep it. */
#define SYSTEM_CLOCK_HZ   16000000UL

/* Millisecond counter, incremented by the SysTick interrupt */
static volatile uint32_t g_millis = 0;

/* ============================================================================
 * Clock_Init - the MCU already runs on the 16 MHz HSI at reset.
 * ----------------------------------------------------------------------------
 * Kept as an explicit no-op so main() reads clearly and so a real clock
 * setup can be dropped in here later without changing call sites.
 * ==========================================================================*/
void Clock_Init(void)
{
    /* Default reset state = HSI 16 MHz selected as system clock. Nothing to do. */
}

/* ============================================================================
 * SysTick_Init - 1 ms tick interrupt
 * ----------------------------------------------------------------------------
 * SysTick is a 24-bit down-counter clocked (here) from the core clock.
 * Reload = clock/1000 - 1 gives a 1 ms period.
 * ==========================================================================*/
void SysTick_Init(void)
{
    SysTick->LOAD = (SYSTEM_CLOCK_HZ / 1000U) - 1U;  // 1 ms reload
    SysTick->VAL  = 0;                               // Clear current value
    SysTick->CTRL = (1 << 0)   // ENABLE
                  | (1 << 1)   // TICKINT  (interrupt on reload)
                  | (1 << 2);  // CLKSOURCE = processor clock
}

/* ============================================================================
 * SysTick_Handler - increments the millisecond counter
 * ==========================================================================*/
void SysTick_Handler(void)
{
    g_millis++;
}

/* ============================================================================
 * Millis - return milliseconds elapsed since SysTick_Init()
 * ==========================================================================*/
uint32_t Millis(void)
{
    return g_millis;
}

/* ============================================================================
 * DelayMs - blocking delay based on the SysTick tick
 * ----------------------------------------------------------------------------
 * Uses unsigned subtraction so it is correct across the 32-bit wrap.
 * ==========================================================================*/
void DelayMs(uint32_t ms)
{
    uint32_t start = g_millis;
    while ((uint32_t)(g_millis - start) < ms);
}

/* ============================================================================
 * Delay - legacy busy-wait (kept for rough short spins only)
 * ==========================================================================*/
void Delay(uint32_t count)
{
    for (volatile uint32_t i = 0; i < count; i++);
}

/* ============================================================================
 * IWDG_Start - start the independent watchdog
 * ----------------------------------------------------------------------------
 * IWDG runs from the ~32 kHz LSI. With prescaler /32 and reload 1250 the
 * timeout is roughly 1250 / (32000/32) = ~1.25 s. If the main loop fails to
 * call IWDG_Refresh() within that window, the MCU resets.
 * ==========================================================================*/
void IWDG_Start(void)
{
    IWDG->KR  = 0x5555;   // Enable register access
    IWDG->PR  = 3;        // Prescaler /32
    IWDG->RLR = 1250;     // Reload -> ~1.25 s timeout
    IWDG->KR  = 0xAAAA;   // Refresh once to load the reload value
    IWDG->KR  = 0xCCCC;   // Start the watchdog
}

/* ============================================================================
 * IWDG_Refresh - kick the watchdog
 * ==========================================================================*/
void IWDG_Refresh(void)
{
    IWDG->KR = 0xAAAA;
}
