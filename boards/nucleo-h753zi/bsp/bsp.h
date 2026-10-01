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