/**
 * @file stm32_console.h
 * @brief The console contract on STM32, for a board that has a console.
 *
 * The contract itself is declared in robohub-common. What is here is what a
 * board's support package needs in order to satisfy it: the rate it decided to
 * run at, and the fact that the pins are already configured, because this
 * platform configures them from the board's own definitions.
 *
 * A board with no console does not call this at all, and a program that writes
 * one gets the contract's error rather than a board that pretends.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "console.h"

/**
 * @brief Open the console.
 *
 * Called by a board's support package during its registration, once the pins
 * named in the board's own header are already alternate functions. The rate and
 * the pins are the board's; this only drives the peripheral.
 *
 * Idempotent: a board that calls it twice gets one open console, not two.
 */
RoboHubConsoleError robohub_console_init(void);

/**
 * @brief This board's console, and whether it is open.
 *
 * Exposed because something has to be able to tell whether writing to the console
 * will work before it writes to it. The fault reporter is the case that matters:
 * it can run at any point in the program's life, including before the board's
 * support package has opened anything, and driving an unopened console faults
 * again inside the handler that was trying to report the first fault.
 */
extern RoboHubConsole robohub_console;

#ifdef __cplusplus
}
#endif