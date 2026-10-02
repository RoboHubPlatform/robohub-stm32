/**
 * @file bsp.h
 * @brief nucleo-l496zg-p — what this board is.
 *
 * The console UART and its clock, and the LEDs. Those are facts about the board
 * and nothing else here is: the platform implements every board of the family,
 * and this says which one it is standing on.
 *
 * Written from this board's own specification rather than by hand, so that a
 * board which is regenerated cannot disagree with the data the build used to
 * generate it.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BSP_H
#define BSP_H

#include "robohub/platform/stm32_hal.h"

/* ==========================================================================
 *  The console
 * ========================================================================== */

/* LPUART1 on GPIOGGPIO_PIN_7 and GPIOGGPIO_PIN_8, which is where ST wires
 * this board's ST-LINK virtual COM port. A program's output is readable without
 * a probe on the pins, which is the only reason to choose a console UART on a
 * board like this one. */
#define BSP_CONSOLE_INSTANCE    LPUART1
#define BSP_CONSOLE_BAUD        115200u
#define BSP_CONSOLE_TX_PORT      GPIOG
#define BSP_CONSOLE_TX_PIN       GPIO_PIN_7
#define BSP_CONSOLE_RX_PORT      GPIOG
#define BSP_CONSOLE_RX_PIN       GPIO_PIN_8
#define BSP_CONSOLE_AF           GPIO_AF8_LPUART1

/* The peripheral clock. Where the part has a kernel clock mux for the console,
 * it is selected here; where it does not — the older M4 parts hang the UART
 * straight off the bus clock — there is nothing to select and the call is left
 * out rather than written and compiling to a symbol that is not there. */
#define BSP_CONSOLE_PERIPH_CLK    RCC_PERIPHCLK_LPUART1
#define BSP_CONSOLE_CLOCK_FIELD    Lpuart1ClockSelection
#define BSP_CONSOLE_CLOCK_SOURCE   RCC_LPUART1CLKSOURCE_HSI

#define BSP_CONSOLE_CLK_ENABLE() __HAL_RCC_LPUART1_CLK_ENABLE()

/* ==========================================================================
 *  The user LEDs
 * ========================================================================== */

#define BSP_LED_GREEN _PORT GPIOB
#define BSP_LED_GREEN _PIN  GPIO_PIN_0
#define BSP_LED_YELLOW_PORT GPIOB
#define BSP_LED_YELLOW_PIN  GPIO_PIN_14
#define BSP_LED_BLUE  _PORT GPIOC
#define BSP_LED_BLUE  _PIN  GPIO_PIN_7

#define BSP_LED_NAME "user_led"

RoboHubError bsp_init(void);
RoboHubError robohub_board_register(void);

#endif /* BSP_H */
