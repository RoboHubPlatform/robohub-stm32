
/*
 * Compiled only when the application asks for it.
 *
 * The symbol comes from the program's own configuration, so a build that
 * does not use this peripheral does not carry the driver for it. The file
 * stays in the module and out of the firmware, which is the difference
 * between a driver that is available and one that is compiled.
 */
#if CONFIG_RH_HAL_SPI


#include "robohub/platform/stm32_spi.h"
#include "robohub/platform/stm32_error.h"

/* -------------------------------------------------------------------------- */
/*  Internals                                                                  */
/* -------------------------------------------------------------------------- */

static RoboHubSTM32SPIContext* context_of(RoboHubSPI* self)
{
    if (self == NULL || self->context == NULL) {
        return NULL;
    }
    return (RoboHubSTM32SPIContext*)self->context;
}

static void enable_clock(SPI_HandleTypeDef* handle)
{
    if (handle->Instance == SPI1) {
        __HAL_RCC_SPI1_CLK_ENABLE();
    } else if (handle->Instance == SPI2) {
        __HAL_RCC_SPI2_CLK_ENABLE();
    }
#if defined(SPI3)
    else if (handle->Instance == SPI3) {
        __HAL_RCC_SPI3_CLK_ENABLE();
    }
#endif
}

/* The peripheral clock an SPI bus runs from. SPI1 and SPI2 are on APB2 and APB1
 * respectively on every family this targets, and getting it wrong scales the
 * requested speed by the bus ratio rather than failing. */
static rh_u32 peripheral_clock(SPI_HandleTypeDef* handle)
{
    if (handle->Instance == SPI2) {
        return HAL_RCC_GetPCLK1Freq();
    }
    return HAL_RCC_GetPCLK2Freq();
}

/* The prescaler nearest the requested speed, without going above it.
 *
 * Going above is refused rather than accepted: an SPI clock faster than the
 * device is rated for is not a faster transfer, it is a device reading the bits
 * it was not sent. The vendor's own macros assert on an out-of-range value, so
 * the range is checked here and the nearest legal divider chosen inside it. */
static rh_u32 prescaler_for(rh_u32 peripheral, rh_u32 speed_hz)
{
    static const rh_u32 divisors[] = {2, 4, 8, 16, 32, 64, 128, 256};
    for (unsigned i = 0; i < sizeof(divisors) / sizeof(divisors[0]); i++) {
        if (peripheral / divisors[i] <= speed_hz) {
            return divisors[i];
        }
    }
    /* Faster than /256. The slowest divider is the slowest there is, and the
     * caller asked for something this peripheral cannot produce. */
    return 0;
}

static rh_u32 vendor_prescaler(rh_u32 divisor)
{
    switch (divisor) {
    case 2:   return SPI_BAUDRATEPRESCALER_2;
    case 4:   return SPI_BAUDRATEPRESCALER_4;
    case 8:   return SPI_BAUDRATEPRESCALER_8;
    case 16:  return SPI_BAUDRATEPRESCALER_16;
    case 32:  return SPI_BAUDRATEPRESCALER_32;
    case 64:  return SPI_BAUDRATEPRESCALER_64;
    case 128: return SPI_BAUDRATEPRESCALER_128;
    case 256: return SPI_BAUDRATEPRESCALER_256;
    default:  return 0;
    }
}

/* -------------------------------------------------------------------------- */
/*  Interface operations                                                       */
/* -------------------------------------------------------------------------- */

static RoboHubError op_init(RoboHubSPI* self, rh_u32 speed_hz)
{
    RoboHubSTM32SPIContext* ctx = context_of(self);
    if (ctx == NULL || ctx->handle == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (speed_hz == 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    rh_u32 peripheral = peripheral_clock(ctx->handle);
    if (peripheral == 0) {
        return ROBOHUB_ERR_STATE;
    }
    rh_u32 divisor = prescaler_for(peripheral, speed_hz);
    rh_u32 prescaler = vendor_prescaler(divisor);
    if (prescaler == 0) {
        return ROBOHUB_ERR_NOT_SUPPORTED;
    }

    enable_clock(ctx->handle);

    /* Mode 0, 8 bits, MSB first, software NSS, no CRC. A board wires the bus to
     * one mode; the mode is what the board's timing was designed against, and
     * a driver that let a caller change it would let one caller change how every
     * other device on the bus is clocked. */
    ctx->handle->Init.Direction = SPI_DIRECTION_2LINES;
    ctx->handle->Init.DataSize = SPI_DATASIZE_8BIT;
    ctx->handle->Init.CLKPolarity = SPI_POLARITY_LOW;
    ctx->handle->Init.CLKPhase = SPI_PHASE_1EDGE;
    ctx->handle->Init.NSS = SPI_NSS_SOFT;
    ctx->handle->Init.BaudRatePrescaler = prescaler;
    ctx->handle->Init.FirstBit = SPI_FIRSTBIT_MSB;
    ctx->handle->Init.TIMode = SPI_TIMODE_DISABLE;
    ctx->handle->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    ctx->handle->Init.CRCPolynomial = 7;

    if (HAL_SPI_Init(ctx->handle) != HAL_OK) {
        return robohub_stm32_to_error(HAL_SPI_Init(ctx->handle));
    }

    ctx->speed_hz = speed_hz;
    return ROBOHUB_OK;
}

static RoboHubError op_transfer(RoboHubSPI* self,
                                const rh_u8* tx,
                                rh_u8* rx,
                                rh_size len)
{
    RoboHubSTM32SPIContext* ctx = context_of(self);
    if (ctx == NULL || ctx->handle == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return ROBOHUB_OK;
    }
    if (tx == NULL && rx == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (ctx->speed_hz == 0) {
        return ROBOHUB_ERR_STATE;
    }

    /* SPI shifts both ways at once, so a transfer with only one buffer still
     * has to clock `len` bytes. The bytes that go out while nobody is listening
     * are discarded into a scratch byte rather than into the caller's memory:
     * writing them into the receive buffer would overwrite what is being
     * received on the very same clock edges. */
    rh_u8 scratch = 0xFFu;
    const rh_u8* out = (tx != NULL) ? tx : &ctx->last_rx_scratch;
    rh_u8* in = (rx != NULL) ? rx : &scratch;

    rh_u32 timeout = ctx->timeout_ms ? ctx->timeout_ms : RH_STM32_SPI_DEFAULT_TIMEOUT_MS;

    /* The chip select is asserted for the whole transfer and released after it,
     * because a device that sees select go high between two bytes treats them as
     * two transfers. */
    if (ctx->chip_select != NULL && ctx->chip_select->write != NULL) {
        ctx->chip_select->write(ctx->chip_select, ctx->chip_select_pin, 0u);
    }

    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(
        ctx->handle, (uint8_t*)out, in, (uint16_t)len, timeout);

    if (ctx->chip_select != NULL && ctx->chip_select->write != NULL) {
        ctx->chip_select->write(ctx->chip_select, ctx->chip_select_pin, 1u);
    }

    return robohub_stm32_to_error(status);
}

/* -------------------------------------------------------------------------- */
/*  Public API                                                                 */
/* -------------------------------------------------------------------------- */

RoboHubError stm32_spi_attach(RoboHubSPI* self,
                              rh_name name,
                              RoboHubSTM32SPIContext* context,
                              SPI_HandleTypeDef* handle,
                              RoboHubGPIO* chip_select,
                              rh_u32 chip_select_pin,
                              rh_u32 timeout_ms)
{
    if (self == NULL || context == NULL || handle == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    /* The context is the caller's, so that two buses on one board do not share
     * a handle. It is zeroed rather than assigned field by field, because the
     * transfer path reads the scratch byte and a context that was never zeroed
     * would shift whatever the stack happened to hold. */
    *context = (RoboHubSTM32SPIContext){
        .handle = handle,
        .chip_select = chip_select,
        .chip_select_pin = chip_select_pin,
        .speed_hz = 0,
        .timeout_ms = timeout_ms,
        .last_rx_scratch = 0xFFu,
    };

    self->name = name;
    self->context = (void*)context;
    self->init = op_init;
    self->transfer = op_transfer;
    return ROBOHUB_OK;
}

#endif /* CONFIG_RH_HAL_SPI */
