/**
 * @file stm32_gpio.c
 * @brief GPIO driver for the RoboHub HAL, on the STM32 HAL.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include "robohub/platform/stm32_gpio.h"
#include "robohub/platform/stm32_hal.h"

// The vendor's HAL, by the name this build declared for it. This driver
// is written against the vendor's constants — GPIO_PIN_SET, the flag and
// status names, the handle types — and none of them exist without this
// header. A driver that did not include it compiled to a build reporting
// those names as undeclared, which says nothing about the header that
// would have defined them, and every such driver failed for the same one
// reason at a different line.

/* -------------------------------------------------------------------------- */
/*  Internals                                                                  */
/* -------------------------------------------------------------------------- */

/* The context of an instance, or NULL when there is none. Every operation
 * checks this before reaching the hardware: an instance that was never attached
 * has no port, and a NULL dereference is a crash the caller cannot report. */
static const RoboHubSTM32GPIOContext* context_of(const RoboHubGPIO* self)
{
    if (self == NULL || self->context == NULL) {
        return NULL;
    }
    return (const RoboHubSTM32GPIOContext*)self->context;
}

/* The GPIO clock for a port. The clock is per-port and a peripheral does not
 * work until it is on, so this is done once per attach rather than on every
 * operation: switching a clock off and on again is not free, and a pin that was
 * configured stays configured across it. */
static void enable_port_clock(GPIO_TypeDef* port)
{
    if (port == GPIOA) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    } else if (port == GPIOB) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    } else if (port == GPIOC) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    } else if (port == GPIOD) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    } else if (port == GPIOE) {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    } else if (port == GPIOF) {
        __HAL_RCC_GPIOF_CLK_ENABLE();
    } else if (port == GPIOG) {
        __HAL_RCC_GPIOG_CLK_ENABLE();
    }
#if defined(GPIOH)
    else if (port == GPIOH) {
        __HAL_RCC_GPIOH_CLK_ENABLE();
    }
#endif
#if defined(GPIOI)
    else if (port == GPIOI) {
        __HAL_RCC_GPIOI_CLK_ENABLE();
    }
#endif
#if defined(GPIOJ)
    else if (port == GPIOJ) {
        __HAL_RCC_GPIOJ_CLK_ENABLE();
    }
#endif
#if defined(GPIOK)
    else if (port == GPIOK) {
        __HAL_RCC_GPIOK_CLK_ENABLE();
    }
#endif
}

/* The vendor's mode for one of ours. A mode this driver does not know is
 * refused rather than approximated: writing a nearby mode to a pin is how a
 * board ends up driving an output that was asked to be an analogue input. */
static int vendor_mode(RoboHubGPIOMode mode, rh_u32 alternate, rh_u32* out)
{
    switch (mode) {
    case RH_GPIO_MODE_INPUT:
        *out = GPIO_MODE_INPUT;
        return 1;
    case RH_GPIO_MODE_OUTPUT:
        *out = GPIO_MODE_OUTPUT_PP;
        return 1;
    case RH_GPIO_MODE_OUTPUT_OPEN_DRAIN:
        /* One name, whatever the family spells it. `stm32_hal.h` defines
         * `GPIO_MODE_OUTPUT_OD` in terms of the older `GPIO_MODE_OD` where that
         * is what the vendor provided, so this driver never has to know which
         * family it is built for — and a family ST ships tomorrow is one more
         * line in that header rather than another branch here. The branch that
         * was here asked a list of family names, and a family missing from it
         * failed as `GPIO_MODE_OD undeclared`, which names neither the family
         * nor the header that would have settled it. */
        *out = GPIO_MODE_OUTPUT_OD;
        return 1;
    case RH_GPIO_MODE_ALTERNATE:
        *out = GPIO_MODE_AF_PP;
        return 1;
    case RH_GPIO_MODE_ANALOG:
        *out = GPIO_MODE_ANALOG;
        return 1;
    default:
        (void)alternate;
        return 0;
    }
}

/* The vendor's pull for one of ours. */
static rh_u32 vendor_pull(RoboHubGPIOPull pull)
{
    switch (pull) {
    case RH_GPIO_PULL_UP:
        return GPIO_PULLUP;
    case RH_GPIO_PULL_DOWN:
        return GPIO_PULLDOWN;
    case RH_GPIO_PULL_NONE:
    default:
        return GPIO_NOPULL;
    }
}

/* Configure a pin. Shared by init and set_mode, because they are the same
 * operation: init is a set_mode on every pin of the instance. */
static RoboHubError configure(const RoboHubSTM32GPIOContext* ctx,
                              rh_u32 pin_mask,
                              rh_u32 mode_value)
{
    GPIO_InitTypeDef init;

    init.Pin = pin_mask;
    init.Mode = mode_value;
    init.Pull = ctx->pull;
    init.Speed = ctx->speed;
    init.Alternate = ctx->alternate;

    HAL_GPIO_Init(ctx->port, &init);
    return ROBOHUB_OK;
}

/* -------------------------------------------------------------------------- */
/*  Interface operations                                                       */
/* -------------------------------------------------------------------------- */

/*
 * An instance is a port and a pin, but the interface asks for a pin number
 * rather than a mask. A pin outside the instance's own mask is refused, because
 * a caller that names pin 7 on an instance bound to pin 5 has a bug, and
 * writing to pin 7 anyway would hide it behind a plausible answer.
 */
static int pin_belongs(const RoboHubSTM32GPIOContext* ctx, rh_u32 pin)
{
    if (pin >= 16) {
        return 0;
    }
    return (ctx->pin_mask & (1u << pin)) != 0;
}

static RoboHubError op_init(RoboHubGPIO* self)
{
    const RoboHubSTM32GPIOContext* ctx = context_of(self);
    if (ctx == NULL || ctx->port == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    enable_port_clock(ctx->port);

    /* Every pin starts as an input. A pin is only driven once something asks
     * for an output, so a pin that is attached but never configured cannot
     * drive the board it is on. */
    return configure(ctx, ctx->pin_mask, GPIO_MODE_INPUT);
}

static RoboHubError op_set_mode(RoboHubGPIO* self, rh_u32 pin, rh_u32 mode)
{
    const RoboHubSTM32GPIOContext* ctx = context_of(self);
    if (ctx == NULL || ctx->port == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (!pin_belongs(ctx, pin)) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    rh_u32 vendor = 0;
    if (!vendor_mode((RoboHubGPIOMode)mode, ctx->alternate, &vendor)) {
        return ROBOHUB_ERR_NOT_SUPPORTED;
    }

    return configure(ctx, 1u << pin, vendor);
}

static RoboHubError op_write(RoboHubGPIO* self, rh_u32 pin, rh_u32 value)
{
    const RoboHubSTM32GPIOContext* ctx = context_of(self);
    if (ctx == NULL || ctx->port == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (!pin_belongs(ctx, pin)) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    /* A value that is neither level is a caller that did not mean one, and
     * writing it low would be indistinguishable from a deliberate low. */
    if (value != RH_GPIO_LOW && value != RH_GPIO_HIGH) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    HAL_GPIO_WritePin(ctx->port, (uint16_t)(1u << pin),
                      (value == RH_GPIO_HIGH) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return ROBOHUB_OK;
}

static RoboHubError op_read(RoboHubGPIO* self, rh_u32 pin, rh_u32* value)
{
    const RoboHubSTM32GPIOContext* ctx = context_of(self);
    if (ctx == NULL || ctx->port == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (value == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (!pin_belongs(ctx, pin)) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    *value = (HAL_GPIO_ReadPin(ctx->port, (uint16_t)(1u << pin)) == GPIO_PIN_SET)
                 ? (rh_u32)RH_GPIO_HIGH
                 : (rh_u32)RH_GPIO_LOW;
    return ROBOHUB_OK;
}

/* -------------------------------------------------------------------------- */
/*  Public API                                                                 */
/* -------------------------------------------------------------------------- */

RoboHubError stm32_gpio_attach(RoboHubGPIO* self,
                               rh_name name,
                               const RoboHubSTM32GPIOContext* context)
{
    if (self == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (context == NULL || context->port == NULL || context->pin_mask == 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    self->name = name;
    self->context = (void*)context;
    self->init = op_init;
    self->set_mode = op_set_mode;
    self->write = op_write;
    self->read = op_read;
    return ROBOHUB_OK;
}

RoboHubError stm32_gpio_set_mode(RoboHubGPIO* self, rh_u32 pin, rh_u32 mode)
{
    if (self == NULL || self->set_mode == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return self->set_mode(self, pin, mode);
}

RoboHubError stm32_gpio_write(RoboHubGPIO* self, rh_u32 pin, rh_u32 value)
{
    if (self == NULL || self->write == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return self->write(self, pin, value);
}

RoboHubError stm32_gpio_read(RoboHubGPIO* self, rh_u32 pin, rh_u32* value)
{
    if (self == NULL || self->read == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return self->read(self, pin, value);
}
