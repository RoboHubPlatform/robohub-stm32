
/*
 * Compiled only when the application asks for it.
 *
 * The symbol comes from the program's own configuration, so a build that
 * does not use this peripheral does not carry the driver for it. The file
 * stays in the module and out of the firmware, which is the difference
 * between a driver that is available and one that is compiled.
 */
#if CONFIG_RH_HAL_I2C


#include "robohub/platform/stm32_i2c.h"
#include "robohub/platform/stm32_error.h"

/* -------------------------------------------------------------------------- */
/*  Internals                                                                  */
/* -------------------------------------------------------------------------- */

static const RoboHubSTM32I2CContext* context_of(const RoboHubI2C* self)
{
    if (self == NULL || self->context == NULL) {
        return NULL;
    }
    return (const RoboHubSTM32I2CContext*)self->context;
}

/* The address as the vendor's HAL wants it: shifted left into the frame.
 *
 * The interface states a 7-bit address because that is what a device's
 * datasheet prints, and the shift is this driver's job. Doing it here is what
 * lets a caller pass the address it read off the datasheet.
 */
static rh_u16 vendor_address(rh_u8 addr)
{
    return (rh_u16)((rh_u16)addr << 1);
}

/* The clock enable for a peripheral.
 *
 * The comparison is on `Instance`, which is the peripheral's own address, and not
 * on the handle. A peripheral name in the vendor's headers is an address rather
 * than an object — `I2C1` is `((I2C_TypeDef *) 0x40005400)` — so taking its
 * address is not something the compiler can do, and a handle is this driver's own
 * object anyway, so its address says nothing about which peripheral it is on.
 *
 * Which peripherals exist is guarded, because a part with only two has no I2C3 and
 * naming it would be a board that does not build for that part.
 */
static void enable_clock(I2C_HandleTypeDef* handle)
{
    if (handle->Instance == I2C1) {
        __HAL_RCC_I2C1_CLK_ENABLE();
    } else if (handle->Instance == I2C2) {
        __HAL_RCC_I2C2_CLK_ENABLE();
    }
#if defined(I2C3)
    else if (handle->Instance == I2C3) {
        __HAL_RCC_I2C3_CLK_ENABLE();
    }
#endif
#if defined(I2C4)
    else if (handle->Instance == I2C4) {
        __HAL_RCC_I2C4_CLK_ENABLE();
    }
#endif
}

/* The peripheral clock, which the bus speed is measured against. */
/* Every I2C peripheral on every family this platform targets is on APB1. The
 * bus differs across families for other peripherals, and a board that names one
 * on the wrong bus is a board whose transfers run at a fraction of the speed
 * asked for, which is a wrong answer rather than a failure. */
static rh_u32 peripheral_clock(I2C_HandleTypeDef* handle)
{
    (void)handle;
    return HAL_RCC_GetPCLK1Freq();
}

#if !defined(ROBOHUB_STM32_FAMILY_H7) && !defined(ROBOHUB_STM32_FAMILY_H5) && \
    !defined(ROBOHUB_STM32_FAMILY_U5) && !defined(ROBOHUB_STM32_FAMILY_L5) && \
    !defined(ROBOHUB_STM32_FAMILY_WB) && !defined(ROBOHUB_STM32_FAMILY_WBA) && \
    !defined(ROBOHUB_STM32_FAMILY_WL)
/* The duty cycle the vendor asks for at a given speed. Above 100 kHz the bus
 * needs the asymmetric timing, because a symmetric one cannot hold SCL high long
 * enough for the slowest device to stretch it. */
static rh_u32 duty_for(rh_u32 speed_hz)
{
    return (speed_hz <= 100000u) ? I2C_DUTYCYCLE_2 : I2C_DUTYCYCLE_16_9;
}
#endif

/* -------------------------------------------------------------------------- */
/*  Interface operations                                                       */
/* -------------------------------------------------------------------------- */

static RoboHubError op_init(RoboHubI2C* self, rh_u32 speed_hz)
{
    RoboHubSTM32I2CContext* ctx = (RoboHubSTM32I2CContext*)context_of(self);
    if (ctx == NULL || ctx->handle == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (speed_hz == 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    /* A bus speed the peripheral clock cannot generate is refused rather than
     * rounded. The vendor's macro asserts on it, and an assert in a driver is
     * a reset on a board nobody is holding. */
    rh_u32 clock = peripheral_clock(ctx->handle);
    if (speed_hz > 1000000u || clock == 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (clock / speed_hz < 2u) {
        return ROBOHUB_ERR_NOT_SUPPORTED;
    }

    enable_clock(ctx->handle);

#if defined(ROBOHUB_STM32_FAMILY_H7) || defined(ROBOHUB_STM32_FAMILY_H5) || \
    defined(ROBOHUB_STM32_FAMILY_U5) || defined(ROBOHUB_STM32_FAMILY_L5) || \
    defined(ROBOHUB_STM32_FAMILY_WB) || defined(ROBOHUB_STM32_FAMILY_WBA) || \
    defined(ROBOHUB_STM32_FAMILY_WL)
    /*
     * These families have the newer I2C peripheral, and it does not take a clock
     * speed. It takes `Timing`: a register encoding the prescaler and the four
     * timings that come out of it, computed from the peripheral clock and the
     * target speed by the vendor's formulas.
     *
     * That computation is not implemented here, and it is not approximated. A
     * timing value that is close but wrong produces a bus that runs at a speed
     * nobody asked for and reports no error at all — a device that is reliable
     * at 400 kHz read unreliably because the bus came up at 370 — which is the
     * one failure a bus driver must not have. So this is refused until the table
     * is written, and a caller is told the family is not supported rather than
     * given a clock it did not ask for.
     */
    (void)clock;
    return ROBOHUB_ERR_NOT_SUPPORTED;
#else
    ctx->handle->Init.ClockSpeed = speed_hz;
    ctx->handle->Init.DutyCycle = duty_for(speed_hz);
    ctx->handle->Init.OwnAddress = 0;
    ctx->handle->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    ctx->handle->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    ctx->handle->Init.OwnAddress1Masks = I2C_OA1_NOMASK;
    ctx->handle->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    ctx->handle->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(ctx->handle) != HAL_OK) {
        return robohub_stm32_to_error(HAL_I2C_Init(ctx->handle));
    }
#endif

    ctx->speed_hz = speed_hz;
    return ROBOHUB_OK;
}

static RoboHubError op_write(RoboHubI2C* self,
                             rh_u8 addr,
                             const rh_u8* data,
                             rh_size len)
{
    const RoboHubSTM32I2CContext* ctx = context_of(self);
    if (ctx == NULL || ctx->handle == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (data == NULL && len != 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return ROBOHUB_OK;
    }
    if (ctx->speed_hz == 0) {
        return ROBOHUB_ERR_STATE;
    }

    rh_u32 timeout = ctx->timeout_ms ? ctx->timeout_ms : RH_STM32_I2C_DEFAULT_TIMEOUT_MS;
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(
        ctx->handle, vendor_address(addr), (uint8_t*)data, (uint16_t)len, timeout);
    return robohub_stm32_to_error(status);
}

static RoboHubError op_read(RoboHubI2C* self,
                            rh_u8 addr,
                            rh_u8* data,
                            rh_size len)
{
    const RoboHubSTM32I2CContext* ctx = context_of(self);
    if (ctx == NULL || ctx->handle == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (data == NULL && len != 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return ROBOHUB_OK;
    }
    if (ctx->speed_hz == 0) {
        return ROBOHUB_ERR_STATE;
    }

    rh_u32 timeout = ctx->timeout_ms ? ctx->timeout_ms : RH_STM32_I2C_DEFAULT_TIMEOUT_MS;
    HAL_StatusTypeDef status = HAL_I2C_Master_Receive(
        ctx->handle, vendor_address(addr), data, (uint16_t)len, timeout);
    return robohub_stm32_to_error(status);
}

/* -------------------------------------------------------------------------- */
/*  Public API                                                                 */
/* -------------------------------------------------------------------------- */

RoboHubError stm32_i2c_attach(RoboHubI2C* self,
                              rh_name name,
                              RoboHubSTM32I2CContext* context)
{
    if (self == NULL || context == NULL || context->handle == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    /* The context is the caller's rather than one of the driver's, because a
     * driver-owned one would be shared by every bus on the board: attaching a
     * second bus would leave the first pointing at the second's peripheral. */
    context->speed_hz = 0;

    self->name = name;
    self->context = (void*)context;
    self->init = op_init;
    self->write = op_write;
    self->read = op_read;
    return ROBOHUB_OK;
}

RoboHubError stm32_i2c_probe(RoboHubI2C* self, rh_u8 addr)
{
    const RoboHubSTM32I2CContext* ctx = context_of(self);
    if (ctx == NULL || ctx->handle == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (ctx->speed_hz == 0) {
        return ROBOHUB_ERR_STATE;
    }

    rh_u32 timeout = ctx->timeout_ms ? ctx->timeout_ms : RH_STM32_I2C_DEFAULT_TIMEOUT_MS;
    /* Two trials: a device that is slow to release the bus after resetting
     * answers on the second and not the first, and one trial would call a
     * present device absent. */
    HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(
        ctx->handle, vendor_address(addr), 2, timeout);
    return robohub_stm32_to_error(status);
}

#endif /* CONFIG_RH_HAL_I2C */
