/**
 * @file main.c
 * @brief hello_world — the same program as the Rho example, written in C.
 *
 * This is the example that shows what the toolchain is for. Nothing here is Rho:
 * the program is C, from `#include` to `main`. Everything a build needs is still
 * declared in this directory's manifest — which presets it is for, which board's
 * LED it blinks, which peripherals it asks for — and the build produces the same
 * image, from the same linker script and the same startup assembly, as the Rho
 * version beside it.
 *
 * And nothing here names an STM32 either. It includes robohub-common's interfaces
 * and calls through them, so the same file builds for this platform and for any
 * other that implements them. The board it ends up on is decided by the manifest
 * and the preset, not by this source.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"
#include "registry.h"
#include "types.h"
#include "robohub/hal/console.h"
#include "robohub/hal/gpio.h"
#include "robohub/hal/timer.h"

/* The same names the Rho program uses: a name for the LED rather than a pin,
 * because the pin is a fact about the board and the name is how a program refers
 * to it across every board that has one. */
#define BSP_LED_NAME "user_led"
#define BSP_TIMER_NAME "system_timer"

/* Written by hand where the Rho version calls `string.from_i32`. A C program has
 * printf, so the standard library is the answer and a generated helper would be a
 * step backwards. */
static void say(rh_u32 seconds)
{
    char line[64];
    const char* prefix = "[";
    const char* suffix = " s] hello from the board, in C\n";
    rh_u32 at = 0;

    while (*prefix != '\0') {
        line[at++] = *prefix++;
    }
    rh_u32 value = seconds;
    char digits[12];
    rh_u32 count = 0;
    if (value == 0) {
        digits[count++] = '0';
    }
    while (value > 0) {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (count > 0) {
        line[at++] = digits[--count];
    }
    while (*suffix != '\0') {
        line[at++] = *suffix++;
    }
    (void)robohub_console_write(line, at);
}

int main(void)
{
    /* The clock, the console, and the board's own peripherals. The one call
     * before anything else, and what it does is the platform's to decide. */
    if (robohub_platform_init() != ROBOHUB_OK) {
        return 1;
    }

    /* Asked for by name. A null handle is the answer when there is none: the
     * registry saying "no such name" is a real answer, and one this program has
     * to be able to see rather than dereference. */
    RoboHubGPIO* led = robohub_get_gpio(BSP_LED_NAME);
    RoboHubTimer* timer = robohub_get_timer(BSP_TIMER_NAME);

    (void)robohub_console_puts("\nHello world, in C\nsays something every second, from here on\n");
    (void)robohub_console_puts("--------------------------------------\n");

    rh_u32 seconds = 0;
    for (;;) {
        seconds++;

        say(seconds);

        /* Toggled with the same second the message reports, so what is read and
         * what is seen agree. An absent handle is not a failure: a board without a
         * user LED still has a clock, and the message still means something. */
        if (led != NULL) {
            (void)led->write(led, 0, seconds % 2u);
        }

        if (timer != NULL) {
            timer->delay_ms(timer, 1000u);
        }
    }
}
