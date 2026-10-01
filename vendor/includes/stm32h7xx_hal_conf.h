/**
  ******************************************************************************
  * @file    stm32h7xx_hal_conf.h
  * @brief   HAL configuration for a Rho build of the STM32H7 HAL.
  *
  * The vendor's umbrella header includes this file by name and does not check
  * whether it exists, so a build that provides none fails to compile on a header
  * nobody wrote. It was previously generated at build time from the vendor's own
  * template, which made a third thing that had to be right for the HAL to compile:
  * the template, the generation, and the part.
  *
  * It is a static file because almost nothing in it varies. The clock values are
  * the vendor's defaults for this family and a target that needs different ones
  * says so on the command line, which is what the `#ifndef` guards are for.
  *
  * What the vendor's driver actually selects on — which peripheral drivers exist —
  * is *not* here. Those macros are in this module's manifest, passed as `-D` on
  * the compile line, so that what is enabled is a fact about what this platform
  * supports rather than a fact about which header happened to be found.
  *
  ******************************************************************************
  */

#ifndef __STM32H7xx_HAL_CONF_H
#define __STM32H7xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* The vendor's parameter assertion.
 *
 * Every driver calls `assert_param` on the arguments it is given. ST's own
 * projects put this in `main.c`, which makes it the application's job and leaves
 * a library that is used by two applications with the assertion written twice.
 * It is defined here instead, as nothing, because `USE_FULL_ASSERT` is not set:
 * the vendor's own definition is the same empty thing, and having it in one place
 * means a build never has to remember to add it. */
#ifndef assert_param
#define assert_param(expr) ((void)0U) /*!< Parameter check, compiled out */
#endif

/* The vendor's definitions: __IO, HAL_StatusTypeDef, the register structures, and
 * everything else the umbrella header's own declarations are written in terms of.
 *
 * ST's own template includes this, so a header written to the same contract
 * assumes the configuration brings it in. Without it the umbrella header's own
 * declarations have no types: `__IO` is undeclared, and everything declared with
 * it follows it out. */
#include "stm32h7xx_hal_def.h"

/*  Clock values.
 *
 *  Each is guarded so a target's spec can override it on the command line. The
 *  defaults are the vendor's own for this family; a board carrying a different
 *  crystal says so in its spec rather than editing a vendor-facing header. */

#ifndef HSI_VALUE
#define HSI_VALUE          ((uint32_t)64000000UL)  /*!< Internal oscillator, Hz */
#endif

#ifndef HSE_VALUE
#define HSE_VALUE          ((uint32_t)25000000UL)  /*!< External oscillator, Hz */
#endif

#ifndef HSE_STARTUP_TIMEOUT
#define HSE_STARTUP_TIMEOUT 100U                    /*!< HSE startup timeout, ms */
#endif

#ifndef VDD_VALUE
#define VDD_VALUE          ((uint32_t)3300UL)      /*!< VDD, mV */
#endif

#ifndef CSI_VALUE
#define CSI_VALUE          ((uint32_t)4000000UL)   /*!< Internal RC oscillator, Hz */
#endif

#ifndef LSE_VALUE
#define LSE_VALUE          ((uint32_t)32768UL)     /*!< External low-speed crystal, Hz */
#endif

/* The vendor's own RCC driver reads this when the system clock is taken from a
 * pin rather than from a crystal, so the configuration has to say what that pin
 * carries even for a board that does not use one: a driver compiled without it
 * fails on a symbol nobody at this level asked about. */
#ifndef EXTERNAL_CLOCK_VALUE
#define EXTERNAL_CLOCK_VALUE  ((uint32_t)12288000UL)
#endif

#ifndef LSE_STARTUP_TIMEOUT
#define LSE_STARTUP_TIMEOUT 5000U                  /*!< LSE startup timeout, ms */
#endif

#ifndef TICK_INT_PRIORITY
#define TICK_INT_PRIORITY  ((uint32_t)(15U))       /*!< Tick interrupt priority */
#endif

/*  Which of the vendor's driver headers exist.
 *
 *  This map is what the vendor's own template has, and the vendor's umbrella
 *  header relies on it: `stm32h7xx_hal.h` does not include the peripheral
 *  headers itself. It declares types — `UART_HandleTypeDef`,
 *  `MPU_Region_InitTypeDef` — that those headers define, so the configuration is
 *  where the inclusions have to happen.
 *
 *  Without this section a build that enabled every driver on the command line
 *  still failed with "unknown type name `UART_HandleTypeDef`": the macros were
 *  defined and nothing in the include tree was checking them. The enable says
 *  which drivers this platform supports; the map is what turns that into
 *  declarations.
 *
 *  Kept in step with `build.defines` in this module's manifest. A macro defined
 *  there with no entry here is a driver this configuration knows are enabled and
 *  has no header for, which is a mismatch worth noticing rather than one this
 *  header can resolve. */

#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32h7xx_hal_rcc.h"
#endif

#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32h7xx_hal_gpio.h"
#endif

#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32h7xx_hal_cortex.h"
#endif

/* The UART header declares a DMA handle in its own structures, so DMA has to come
 * first: a header that names a type another header defines is only safe to read
 * after that other header has been read, and the vendor's drivers rely on the
 * order in this map. */
#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32h7xx_hal_dma.h"
#endif

#ifdef HAL_UART_MODULE_ENABLED
#include "stm32h7xx_hal_uart.h"
#endif

#ifdef HAL_USART_MODULE_ENABLED
#include "stm32h7xx_hal_usart.h"
#endif

#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32h7xx_hal_pwr.h"
#endif

#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32h7xx_hal_flash.h"
#endif

#ifdef __cplusplus
}
#endif

#endif /* __STM32H7xx_HAL_CONF_H */
