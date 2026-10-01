/**
 * @file stm32_systick.c
 * @brief The system tick, which is what makes time observable on a board that has
 *        no clock of its own.
 *
 * The HAL starts SysTick from `HAL_Init()` and counts in it. Nothing else: a
 * program that asks how long it has been running, or that waits, is answered by
 * this counter and by nothing else.
 *
 * It has to exist. The vendor's startup assembly declares the symbol weak and
 * points it at its own default handler, which is an instruction to spin. So a
 * board with no SysTick handler boots, runs, and is stopped a millisecond later
 * by a tick it cannot acknowledge — with no fault recorded anywhere, because
 * nothing faulted. The console never opens, because opening it happens after
 * this.
 */

#include "robohub/platform/stm32_hal.h"

/**
 * @brief The interrupt the system tick arrives on.
 *
 * Named by the vendor, in the vector table the linker script places, and nowhere
 * else. The name is not a choice: it is what the hardware reads the table to
 * find.
 */
void SysTick_Handler(void);

/**
 * @brief Count the tick.
 *
 * `HAL_IncTick` is the HAL's, and it is what every `HAL_Delay` and every timeout
 * in the HAL reads. Counting anywhere else would be a second clock that agrees
 * with the first one until it does not.
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
