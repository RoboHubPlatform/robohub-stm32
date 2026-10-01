/**
 * @file bsp.h
 * @brief The Nucleo-H753ZI's board support package.
 *
 * Everything that is a fact about *this board* rather than about STM32s in
 * general: which UART is the console, which pins it is on, which LED, and what
 * the crystal is. A driver cannot know these — they are the board's, and the same
 * driver serves every board — so the board states them here and a program that
 * knows which board it is on gets the right ones.
 *
 * The split matters in both directions. A program that hardcoded `USART3` would
 * work here and nowhere else; a driver that asked the board would work anywhere.
 * So the board owns the handles, the drivers use whatever they are given, and
 * `robohub_platform_init` is what puts the two together.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "error.h"
#include "types.h"

/* -------------------------------------------------------------------------- */
/*  The console                                                                */
/* -------------------------------------------------------------------------- */
/*
 * USART3 on PD8 and PD9, which ST wires to the ST-LINK's virtual COM port. That
 * is what makes a program's output readable without a probe on the pins, which is
 * the only reason to choose a console UART on a board like this one.
 *
 * 115200 8N1, because that is what the ST-LINK's virtual COM port presents; a
 * rate the board does not use shows as nothing at all rather than as rubbish,
 * which is a confusing way to find out.
 */
#define BSP_CONSOLE_BAUD 115200u
#define BSP_CONSOLE_TX_PORT GPIOD
#define BSP_CONSOLE_TX_PIN GPIO_PIN_8
#define BSP_CONSOLE_RX_PORT GPIOD
#define BSP_CONSOLE_RX_PIN GPIO_PIN_9
#define BSP_CONSOLE_AF GPIO_AF7_USART3
#define BSP_CONSOLE_INSTANCE USART3

/* The console UART's kernel clock.
 *
 * On this part the USART2/3/4/5/6/7/8 peripheral clock does not follow PCLK1: it
 * is selected by its own mux in RCC_D2CCIP2R, and it keeps whatever the reset
 * value was until something selects it.
 *
 * That is this board's problem rather than the platform's, and it is here for the
 * same reason the pins are: which UART a board carries, and what that UART hangs
 * off, are facts about the board. A platform that selected a UART clock would be
 * a platform that knew which UART a board uses.
 *
 * HSI rather than a bus clock because HSI is the one reference that does not
 * move — it is what the part boots on, and it is unaffected by the main PLL or by
 * the dividers above it. A console should not change rate because something else
 * was reconfigured.
 */
#define BSP_CONSOLE_PERIPHCLK RCC_PERIPHCLK_USART3
#define BSP_CONSOLE_CLKSOURCE_HSI RCC_USART3CLKSOURCE_HSI

/* -------------------------------------------------------------------------- */
/*  The user LEDs                                                              */
/* -------------------------------------------------------------------------- */
/*
 * The three LEDs a Nucleo-144 board carries, on their usual pins. Only the green
 * one is a signal the program should rely on; the other two exist because the
 * board has them.
 */
#define BSP_LED_GREEN_PORT GPIOB
#define BSP_LED_GREEN_PIN GPIO_PIN_0
#define BSP_LED_YELLOW_PORT GPIOE
#define BSP_LED_YELLOW_PIN GPIO_PIN_1
#define BSP_LED_RED_PORT GPIOB
#define BSP_LED_RED_PIN GPIO_PIN_14

/* The name the platform registers the user LED under. A program asks for this
 * name rather than for a pin, which is what lets the same program run on a board
 * whose LED is elsewhere. */
#define BSP_LED_NAME "user_led"

/* -------------------------------------------------------------------------- */
/*  The clock                                                                  */
/* -------------------------------------------------------------------------- */
/*
 * 25 MHz, from the crystal the board is fitted with — not the 8 MHz an MCO
 * supplies, and not the value the vendor's template states. The clock tree is
 * built from this number, so a wrong one produces a core running at a frequency
 * nobody asked for.
 */
#define BSP_HSE_HZ 25000000u
#define BSP_SYSCLK_HZ 480000000u

/* -------------------------------------------------------------------------- */
/*  Initialisation                                                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Bring the board up: clock, console pins, console, LED pins.
 *
 * Called from the platform's initialisation, after the HAL's own. It does no
 * peripheral configuration of its own beyond pins — the drivers own their
 * peripherals, and this owns only what is the board's.
 */
RoboHubError bsp_init(void);

#ifdef __cplusplus
}
#endif