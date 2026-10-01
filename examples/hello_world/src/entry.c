/**
 * @file entry.c
 * @brief The program's entry point, while the Rho compiler's output is not yet
 *        part of this build.
 *
 * A Rho program is transpiled to C and the result is the application: it brings
 * the platform up through the contract, does the program's work, and returns.
 * Until that translation is in this build, this is the application's `main`, and
 * it does what the program in `main.rho` says — which is a fair test of the
 * whole path around it, because everything it calls is the contract, not a
 * peripheral.
 *
 * It is not a replacement for the generated code and it is not the long answer.
 * It exists so that the rest of the chain — the preset, the fetched CMSIS and
 * HAL, the family's clock, the board's BSP, the console, the linker script — can
 * be brought up and proved on a board before anything depends on it.
 *
 * SPDX-License-Identifier: MIT
 */

#include "platform.h"
#include "console.h"
#include "gpio.h"
#include "timer.h"
#include "registry.h"
#include <stdio.h>

int main(void)
{
    /* The contract's own entry point: the registries, then this board. */
    if (robohub_platform_init() != ROBOHUB_OK) {
        return 1;
    }

    RoboHubGPIO* led = robohub_get_gpio("user_led");
    RoboHubTimer* timer = (RoboHubTimer*)robohub_get_timer("system_timer");
    if (timer == NULL) {
        printf("no timebase, so no seconds to count\n");
        for (;;) {
        }
    }

    printf("\n");
    printf("Hello world\n");
    printf("says something every second, from here on\n");
    printf("--------------------------------------\n");

    rh_u32 seconds = 0;
    for (;;) {
        seconds++;
        printf("[%6lu s] hello from the board\n", (unsigned long)seconds);
        if (led != NULL && led->write != NULL) {
            (void)led->write(led, 0u, (seconds % 2u) == 0u ? 0u : 1u);
        }
        timer->delay_ms(timer, 1000u);
    }
}
