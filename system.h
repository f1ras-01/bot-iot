/*
 * system.h
 * ----------------------------------------------------------------------------
 * System clock, SysTick millisecond timebase, watchdog, and delays.
 * The MCU runs on its default internal 16 MHz HSI oscillator.
 * ----------------------------------------------------------------------------
 */
#ifndef SYSTEM_H
#define SYSTEM_H

#include "stm32f4xx.h"

/* Clock setup. The MCU already runs on the 16 MHz HSI at reset, so this is
 * an explicit no-op kept for clarity. Call it first in main(). */
void Clock_Init(void);

/* Configure SysTick for a 1 ms interrupt tick. Call after Clock_Init(). */
void SysTick_Init(void);

/* Free-running millisecond counter since SysTick_Init(). */
uint32_t Millis(void);

/* Blocking delay in milliseconds, based on the SysTick tick (timeout-safe). */
void DelayMs(uint32_t ms);

/* Legacy busy-wait delay (used only where a rough sub-ms spin is acceptable). */
void Delay(uint32_t count);

/* Start the independent watchdog (IWDG). Must be refreshed via IWDG_Refresh(). */
void IWDG_Start(void);

/* Refresh (kick) the watchdog. Call regularly from the main loop. */
void IWDG_Refresh(void);

#endif /* SYSTEM_H */
