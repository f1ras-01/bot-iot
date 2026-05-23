/*
 * system.h
 * ----------------------------------------------------------------------------
 * System clock (HSE), SysTick millisecond timebase, watchdog, and delays.
 * ----------------------------------------------------------------------------
 */
#ifndef SYSTEM_H
#define SYSTEM_H

#include "stm32f4xx.h"

/* Initialize external 8 MHz oscillator as system clock */
void HSE_Init(void);

/* Configure SysTick for a 1 ms interrupt tick. Call after HSE_Init(). */
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
