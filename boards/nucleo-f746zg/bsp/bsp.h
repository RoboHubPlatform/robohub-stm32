/**
 * @file bsp.h
 * @brief nucleo-f746zg — what this board is.
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


#include "error.h"
#include "types.h"
#include "robohub/platform/stm32_hal.h"

/* ==========================================================================
 *  The console
 * ========================================================================== */

/* USART3 on GPIODGPIO_PIN_8 and GPIODGPIO_PIN_9, which is where ST wires
 * this board's ST-LINK virtual COM port. A program's output is readable without
 * a probe on the pins, which is the only reason to choose a console UART on a
 * board like this one. */
#define BSP_CONSOLE_INSTANCE    USART3
#define BSP_CONSOLE_BAUD        115200u
#define BSP_CONSOLE_TX_PORT      GPIOD
#define BSP_CONSOLE_TX_PIN       GPIO_PIN_8
#define BSP_CONSOLE_RX_PORT      GPIOD
#define BSP_CONSOLE_RX_PIN       GPIO_PIN_9
#define BSP_CONSOLE_AF           GPIO_AF7_USART3

/* The peripheral clock. Where the part has a kernel clock mux for the console,
 * it is selected here; where it does not — the older M4 parts hang the UART
 * straight off the bus clock — there is nothing to select and the call is left
 * out rather than written and compiling to a symbol that is not there. */
#define BSP_CONSOLE_PERIPH_CLK    RCC_PERIPHCLK_USART3
#define BSP_CONSOLE_CLOCK_FIELD    Usart3ClockSelection
#define BSP_CONSOLE_CLOCK_SOURCE   RCC_USART3CLKSOURCE_HSI

#define BSP_CONSOLE_CLK_ENABLE() __HAL_RCC_USART3_CLK_ENABLE()

/* ==========================================================================
 *  The user LEDs
 * ========================================================================== */

#define BSP_LED_GREEN_PORT GPIOB
#define BSP_LED_GREEN_PIN GPIO_PIN_0
#define BSP_LED_YELLOW_PORT GPIOB
#define BSP_LED_YELLOW_PIN  GPIO_PIN_7
#define BSP_LED_RED_PORT GPIOB
#define BSP_LED_RED_PIN GPIO_PIN_14

#define BSP_LED_NAME "user_led"

RoboHubError bsp_init(void);
RoboHubError robohub_board_register(void);

#endif /* BSP_H */
