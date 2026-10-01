/**
 * @file bsp.c
 * @brief The Nucleo-H753ZI's board support package.
 *
 * Pins and a clock. Everything else a program needs is a driver's business, and
 * everything here is a fact about this board that a driver cannot know.
 *
 * SPDX-License-Identifier: MIT
 */

#include "bsp.h"

#include "robohub/platform/stm32_hal.h"
#include "registry.h"
#include "robohub/platform/stm32_platform.h"
#include "robohub/platform/stm32_console.h"
#include "robohub/platform/stm32_platform.h"
#include "robohub/platform/stm32_gpio.h"
#include "robohub/platform/stm32_timer.h"

/* The LED, bound to the pin this board puts it on. A program asks the registry
 * for `user_led` and gets this; it never sees a pin. */
static RoboHubSTM32GPIOContext led_context = {
    .port = BSP_LED_GREEN_PORT,
    .pin_mask = BSP_LED_GREEN_PIN,
    .pull = 0,
    .speed = 0,
    .alternate = 0,
};
static RoboHubGPIO led;
static RoboHubSTM32TimerContext timer_context = {0};
static RoboHubTimer timer;

/* Configure a pair of alternate-function pins for a peripheral. */
static void af_pin(GPIO_TypeDef* port, rh_u32 pin, rh_u32 alternate)
{
    GPIO_InitTypeDef init = {0};
    init.Pin = pin;
    init.Mode = GPIO_MODE_AF_PP;
    /* Pulled up, as the board's own clock configuration has them: the ST-LINK
     * holds the receive line and a pulled-up pin is the idle state rather than
     * whatever the pad floats to. */
    init.Pull = GPIO_PULLUP;
    init.Speed = GPIO_SPEED_HIGH;
    init.Alternate = alternate;
    HAL_GPIO_Init(port, &init);
}

/* Configure a pin as a plain output, which is all an LED needs. */
static void output_pin(GPIO_TypeDef* port, rh_u32 pin)
{
    GPIO_InitTypeDef init = {0};
    init.Pin = pin;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_NOPULL;
    /* Low: an LED does not need to be driven fast, and the slowest grade is the
     * one that costs the least in power on a board that spends its life idle. */
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &init);
}

RoboHubError bsp_init(void)
{
    /* The ports this board's console and LEDs sit on. Clocked here rather than in
     * the drivers, because a driver that switched on the clock for the port a
     * board happened to choose would be a driver that knew about boards. */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* The console's peripheral clock, because the console is this board's and
     * the clock it runs on is the one its UART hangs off. */
    __HAL_RCC_USART3_CLK_ENABLE();

    /* The console. Both directions are brought up even though output is all a
     * program needs today, because a board whose console cannot receive is a
     * board that has to be changed before anything can read from it. */
    af_pin(BSP_CONSOLE_TX_PORT, BSP_CONSOLE_TX_PIN, BSP_CONSOLE_AF);
    af_pin(BSP_CONSOLE_RX_PORT, BSP_CONSOLE_RX_PIN, BSP_CONSOLE_AF);

    output_pin(BSP_LED_GREEN_PORT, BSP_LED_GREEN_PIN);
    output_pin(BSP_LED_YELLOW_PORT, BSP_LED_YELLOW_PIN);
    output_pin(BSP_LED_RED_PORT, BSP_LED_RED_PIN);

    /* Off, so that the first thing a program does is visible rather than the
     * board having come up already lit. */
    HAL_GPIO_WritePin(BSP_LED_GREEN_PORT, BSP_LED_GREEN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BSP_LED_YELLOW_PORT, BSP_LED_YELLOW_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BSP_LED_RED_PORT, BSP_LED_RED_PIN, GPIO_PIN_RESET);

    return ROBOHUB_OK;
}

RoboHubError robohub_board_register(void)
{
    /* The clock comes first: everything below is measured against it, and a
     * peripheral configured against the reset clock runs at the reset clock. The
     * platform owns it because it is the family's, not this board's. */
    stm32_platform_clock_config();

    (void)bsp_init();
    (void)robohub_console_init();

    /* The LED, registered under the name a program asks for. */
    (void)stm32_gpio_attach(&led, BSP_LED_NAME, &led_context);
    (void)robohub_register_gpio(led.name, &led);

    /* The timebase. */
    (void)stm32_timer_start();
    (void)stm32_timer_attach(&timer, "system_timer", &timer_context);
    (void)robohub_register_timer(timer.name, &timer);

    return ROBOHUB_OK;
}
