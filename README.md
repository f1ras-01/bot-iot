# STM32 Bluetooth Robot — Modular Source Layout

The original single `main.c` has been split by peripheral. Each module is a
`.h` (public interface) + `.c` (implementation) pair.

## Files

| Module        | Files                      | Responsibility                                      |
|---------------|----------------------------|-----------------------------------------------------|
| Config        | `Inc/config.h`             | Hardware constants (DS1621 addresses, buffer size). |
| System        | `system.h` / `system.c`    | HSE clock, SysTick 1 ms tick, `DelayMs`, IWDG.      |
| GPIO          | `gpio.h` / `gpio.c`        | All pin configuration + EXTI0 (button) setup.       |
| ADC           | `adc.h` / `adc.c`          | ADC1 3-channel scan mode + DMA2 Stream0 + DMA ISR.  |
| Timers        | `timers.h` / `timers.c`    | TIM2 (ADC trigger) + TIM3 (motor PWM).              |
| I2C           | `i2c.h` / `i2c.c`          | Low-level I2C1 master driver.                       |
| DS1621        | `ds1621.h` / `ds1621.c`    | Temperature sensor (built on the I2C driver).       |
| USART         | `usart.h` / `usart.c`      | USART2 Bluetooth driver (init + send helpers).      |
| Motor         | `motor.h` / `motor.c`      | `SetMotorDirection()`, `EmergencyStop()`.           |
| App           | `app.h` / `app.c`          | Shared globals, `SendADCValues()`, button + USART ISRs. |
| Main          | `Src/main.c`               | Peripheral init + Bluetooth command loop.           |

## Notes

- **`app.c`** holds the `EXTI0_IRQHandler` (button) and `USART2_IRQHandler`
  because they touch shared state. The ADC's DMA interrupt
  (`DMA2_Stream0_IRQHandler`) lives in `adc.c` with the rest of the ADC driver.
- **Shared globals** (`speed`, `temperature`, `adc_values`, `rxBuffer`, ...) are
  defined once in `app.c` and declared `extern` in `app.h`.
- No driver depends on `app.h` except `motor.c` (needs `speed`) and `adc.c`
  (needs `adc_values`/`adc_ready` as the DMA destination).

## Building

Add all `Src/*.c` files to your project and put `Inc/` on the include path.
For STM32CubeIDE / Keil: drop `Inc/` into the include directories and add the
`Src/` files to the build. Each `.c` compiles independently.

## Robustness fixes applied

The following significant issues from the original code were corrected:

1. **Boot hang if sensor absent** — all I2C waits are now bounded by timeouts;
   `DS1621_Init()` returns an error instead of hanging, so the robot boots
   even with no temperature sensor connected.
2. **Blocking I/O inside ISRs** — `EXTI0_IRQHandler` no longer sends UART data;
   it sets `button_event` and the main loop does the work. Keeps interrupt
   latency short and avoids losing USART bytes.
3. **Motor command behaviour** — motors run until an explicit Stop (`S`)
   command. (The earlier time-based failsafe was removed: it assumed the
   controller streams commands continuously, but the app sends one-shot
   commands, so it stopped the motors ~1 s into every move.)
4. **Real timebase** — SysTick provides a 1 ms tick; `DelayMs()` / `Millis()`
   replace the calibration-dependent busy-loop `Delay()`.
5. **RX buffer race** — the command is copied out and the buffer state cleared
   inside a short interrupt-masked critical section.
6. **Watchdog** — the IWDG resets the MCU if the main loop ever stalls.
7. **Negative temperatures** — the DS1621 MSB is decoded as signed
   (`int8_t`), so sub-zero readings are correct.
8. **No float `printf`** — temperature/voltage are formatted with integer
   math, removing the `%f` linker dependency.

## ADC DMA optimization

The ADC was moved from a per-channel interrupt to a DMA-fed design:

- The ADC runs in **scan mode**; each TIM2 TRGO converts all 3 channels.
- **DMA2 Stream0, Channel 0** copies every result directly into `adc_values[]`
  (16-bit, memory-increment on, **circular** mode so it refills every sweep).
- The CPU is interrupted **once per 3-channel sweep** (DMA transfer-complete)
  instead of once per channel — and the software `channel_index` is gone,
  since the DMA places each value in the correct slot.
- `DMA2_Stream0_IRQHandler` (in `adc.c`) just sets `adc_ready`.

`Delay()` is kept in `system.h` for backward compatibility but is no longer
used by the project.
