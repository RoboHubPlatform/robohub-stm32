/**
 * @file hal_conf.h
 * @brief The HAL configuration this firmware is built against.
 *
 * Which of the vendor's peripheral drivers are compiled in is decided here and
 * nowhere else. The module includes at the bottom are not optional: the HAL
 * header refuses to define its own types until something has included
 * stm32h7xx_hal_def.h, and every module header includes it. A configuration that
 * enabled a module without including it produces a HAL that does not compile.
 *
 * It is generated from the vendor's own template, which is the one that carries
 * every clock macro the HAL expects. Hand-writing one is how a build ends up
 * missing HSI_VALUE and failing on a line that says nothing about what was
 * forgotten.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef STM32H7XX_HAL_CONF_H
#define STM32H7XX_HAL_CONF_H

#define HAL_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

/* The Nucleo-144 boards are fitted with a 25 MHz crystal, which is the value the
 * vendor's template states and the value the clock tree below is built for. */
#ifndef HSE_VALUE
#define HSE_VALUE 25000000U
#endif
#define HSE_STARTUP_TIMEOUT 100U

/* The rest of the oscillators the clock tree is built from. A platform brings up
 * one of them and the HAL reports the others, so the numbers have to be here even
 * when nothing in this program asks for them: a missing one is a compile error in
 * the middle of the vendor's clock code, which says nothing about what was
 * forgotten. */
#ifndef HSI_VALUE
#define HSI_VALUE 64000000U     /* 64 MHz, the H7's default internal oscillator */
#endif
#ifndef CSI_VALUE
#define CSI_VALUE 4000000U      /* 4 MHz, the low-power internal RC */
#endif
#ifndef LSI_VALUE
#define LSI_VALUE 32000U        /* 32 kHz, the low-power internal RC */
#endif
#ifndef LSE_VALUE
#define LSE_VALUE 32768U        /* 32.768 kHz, the low-speed external crystal */
#endif
#ifndef LSE_STARTUP_TIMEOUT
#define LSE_STARTUP_TIMEOUT 5000U
#endif
#ifndef EXTERNAL_CLOCK_VALUE
#define EXTERNAL_CLOCK_VALUE 12288000U
#endif
#define VDD_VALUE 3300U
#define TICK_INT_PRIORITY 0x0FU
#define USE_RTOS 0U
#define PREFETCH_ENABLE 1U
#define INSTRUCTION_CACHE_ENABLE 1U
#define DATA_CACHE_ENABLE 1U
#define USE_HAL_ADC_REGISTER_CALLBACKS 0U

/* Every module the firmware uses, and the pins it uses them on, is declared in
 * the board's own support package. This file is what makes the HAL headers
 * usable; it is not the board's configuration. */

#define assert_param(expr) ((void)0U)

#include "stm32h7xx_hal_rcc.h"
#include "stm32h7xx_hal_gpio.h"
#include "stm32h7xx_hal_cortex.h"
#include "stm32h7xx_hal_dma.h"
#include "stm32h7xx_hal_flash.h"
#include "stm32h7xx_hal_pwr.h"
#include "stm32h7xx_hal_uart.h"

#endif /* STM32H7XX_HAL_CONF_H */
