/**
 * @file stm32_gpio.h
 * @brief GPIO driver for the RoboHub HAL, on the STM32 HAL.
 *
 * A GPIO instance is a port and a pin. The driver keeps the port base and the
 * pin in a context, and the RoboHub interface's operations are thin wrappers
 * over the vendor's GPIO calls, so that a board describes a pin and does not
 * carry the peripheral setup itself.
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

#include "error.h"
#include "types.h"
#include "robohub/hal/gpio.h"
#include "robohub/platform/stm32_hal.h"

/* -------------------------------------------------------------------------- */
/*  Pin modes                                                                  */
/* -------------------------------------------------------------------------- */
/*
 * The RoboHub GPIO interface takes a mode as a number but does not say what the
 * numbers are, so they are stated here.
 *
 * These are platform-independent — an input is an input whatever it is wired
 * to — so they belong in robohub-common beside the interface that takes them.
 * They are defined here so that this platform is usable now; a board written
 * against these values keeps working when they move, because the values are
 * part of what this header promises.
 */
typedef enum {
    RH_GPIO_MODE_INPUT = 0,          /* read the pin, no output driver */
    RH_GPIO_MODE_OUTPUT = 1,         /* drive the pin push-pull */
    RH_GPIO_MODE_OUTPUT_OPEN_DRAIN = 2, /* drive low or release, for I2C and one-wire */
    RH_GPIO_MODE_ALTERNATE = 3,      /* the pin belongs to a peripheral */
    RH_GPIO_MODE_ANALOG = 4          /* the pin belongs to an ADC or DAC */
} RoboHubGPIOMode;

/* Pull. A floating input reads whatever the pin happens to pick up, which is a
 * number rather than a decision; a pull is what makes it one. */
typedef enum {
    RH_GPIO_PULL_NONE = 0,
    RH_GPIO_PULL_UP = 1,
    RH_GPIO_PULL_DOWN = 2
} RoboHubGPIOPull;

/* Pin levels. The interface writes a number rather than asking for HIGH or LOW,
 * and these are the numbers it means. */
typedef enum {
    RH_GPIO_LOW = 0,
    RH_GPIO_HIGH = 1
} RoboHubGPIOLevel;

/* -------------------------------------------------------------------------- */
/*  Context                                                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief What a GPIO instance is bound to.
 *
 * A board declares one of these per pin and hands it to `stm32_gpio_attach`. It
 * is const because the driver reads it and never writes it: the pin an instance
 * answers for is fixed when the board is written down, and a driver that could
 * change it would let two instances end up on one pin without anybody asking.
 */
typedef struct {
    GPIO_TypeDef* port;
    /** Pin mask, in the vendor's form: a single pin is `1u << n`. A port-wide
     *  mask is allowed, so one instance may serve every pin on a port. */
    rh_u32 pin_mask;
    /** Pull applied on every `set_mode`. GPIO_NOPULL leaves the pin floating. */
    rh_u32 pull;
    /** Speed grade for outputs, in the vendor's form. Zero selects the slowest,
     *  which is right for a signal and wrong for a clock. */
    rh_u32 speed;
    /** Alternate function index, for RH_GPIO_MODE_ALTERNATE. */
    rh_u32 alternate;
} RoboHubSTM32GPIOContext;

/* -------------------------------------------------------------------------- */
/*  Public API                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Bind a RoboHub GPIO instance to a port and a pin.
 *
 * This fills in the interface's operations and its context; it does not touch
 * the hardware. The pin is configured when `init` is called, so a board may
 * attach every pin it has and initialise only the ones a build uses.
 *
 * @param self     the instance to bind, usually a static in board code
 * @param name     the name it is registered under, e.g. "GPIO_LED"
 * @param context  the pin binding, which must outlive the instance
 */
RoboHubError stm32_gpio_attach(RoboHubGPIO* self,
                               rh_name name,
                               const RoboHubSTM32GPIOContext* context);

/**
 * @brief Configure a pin as an input or an output.
 *
 * @param mode one of RoboHubGPIOMode.
 */
RoboHubError stm32_gpio_set_mode(RoboHubGPIO* self, rh_u32 pin, rh_u32 mode);

/**
 * @brief Drive a pin.
 *
 * @param value RH_GPIO_LOW or RH_GPIO_HIGH.
 */
RoboHubError stm32_gpio_write(RoboHubGPIO* self, rh_u32 pin, rh_u32 value);

/**
 * @brief Read a pin.
 *
 * @param value receives RH_GPIO_LOW or RH_GPIO_HIGH.
 */
RoboHubError stm32_gpio_read(RoboHubGPIO* self, rh_u32 pin, rh_u32* value);

#ifdef __cplusplus
}
#endif
