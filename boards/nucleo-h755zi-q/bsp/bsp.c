/**
 * @file bsp.c
 * @brief nucleo-h755zi-q — what this board is.
 *
 * Generated from this board's specification. The only board-specific content is
 * what the specification says: which UART is the console, which pins it is on,
 * where its clock comes from, and which pins the LEDs are.
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "bsp.h"

#include "registry.h"
#include "robohub/hal/gpio.h"
#include "robohub/hal/timer.h"
#include "robohub/platform/stm32_console.h"
#include "robohub/platform/stm32_gpio.h"
#include "robohub/platform/stm32_platform.h"
#include "robohub/platform/stm32_timer.h"

static RoboHubSTM32GPIOContext led_context = {0};
static RoboHubGPIO led;
static RoboHubSTM32TimerContext timer_context = {0};
static RoboHubTimer timer;

static void af_pin(GPIO_TypeDef* port, uint32_t pin, uint32_t alternate)
{
    GPIO_InitTypeDef init = {0};
    init.Pin = pin;
    init.Mode = GPIO_MODE_AF_PP;
    init.Pull = GPIO_PULLUP;
    init.Speed = GPIO_SPEED_HIGH;
    init.Alternate = alternate;
    HAL_GPIO_Init(port, &init);
}

static void output_pin(GPIO_TypeDef* port, uint32_t pin)
{
    GPIO_InitTypeDef init = {0};
    init.Pin = pin;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &init);
}

RoboHubError bsp_init(void)
{
    /* This part has a kernel clock mux for the console, so it is selected here.
     * Without it the UART is programmed for a rate nobody asked for: the HAL
     * divides by whatever the mux reports, and an unselected mux reports the
     * reset value. */
    RCC_PeriphCLKInitTypeDef peripheral;
    memset(&peripheral, 0, sizeof peripheral);
    peripheral.PeriphClockSelection = BSP_CONSOLE_PERIPH_CLK;
    peripheral.Usart234578ClockSelection = BSP_CONSOLE_CLOCK_SOURCE;
    if (HAL_RCCEx_PeriphCLKConfig(&peripheral) != HAL_OK) {
        return ROBOHUB_ERROR;
    }

    /* The ports this board's console and LEDs sit on, clocked here rather than in
     * the drivers: a driver that switched on the clock for the port a board
     * happened to choose would be a driver that knew about boards. */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    BSP_CONSOLE_CLK_ENABLE();

    af_pin(BSP_CONSOLE_TX_PORT, BSP_CONSOLE_TX_PIN, BSP_CONSOLE_AF);
    af_pin(BSP_CONSOLE_RX_PORT, BSP_CONSOLE_RX_PIN, BSP_CONSOLE_AF);

    output_pin(BSP_LED_GREEN_PORT, BSP_LED_GREEN_PIN);
    output_pin(BSP_LED_YELLOW_PORT, BSP_LED_YELLOW_PIN);
    output_pin(BSP_LED_RED_PORT, BSP_LED_RED_PIN);
    HAL_GPIO_WritePin(BSP_LED_GREEN_PORT, BSP_LED_GREEN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BSP_LED_YELLOW_PORT, BSP_LED_YELLOW_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BSP_LED_RED_PORT, BSP_LED_RED_PIN, GPIO_PIN_RESET);

    led_context.port = BSP_LED_GREEN_PORT;
    led_context.pin_mask = BSP_LED_GREEN_PIN;

    return ROBOHUB_OK;
}

RoboHubError robohub_board_register(void)
{
    stm32_platform_clock_config();

    (void)bsp_init();
    (void)robohub_console_init();

    (void)stm32_gpio_attach(&led, BSP_LED_NAME, &led_context);
    (void)robohub_register_gpio(led.name, &led);

    (void)stm32_timer_start();
    (void)stm32_timer_attach(&timer, "system_timer", &timer_context);
    (void)robohub_register_timer(timer.name, &timer);

    return ROBOHUB_OK;
}
