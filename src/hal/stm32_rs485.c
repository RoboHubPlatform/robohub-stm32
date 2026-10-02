
/*
 * Compiled only when the application asks for it.
 *
 * The symbol comes from the program's own configuration, so a build that
 * does not use this peripheral does not carry the driver for it. The file
 * stays in the module and out of the firmware, which is the difference
 * between a driver that is available and one that is compiled.
 */
#if CONFIG_RH_HAL_RS485


#include "robohub/platform/stm32_rs485.h"
#include "robohub/platform/stm32_hal.h"

// The vendor's HAL, by the name this build declared for it. This driver
// is written against the vendor's constants — GPIO_PIN_SET, the flag and
// status names, the handle types — and none of them exist without this
// header. A driver that did not include it compiled to a build reporting
// those names as undeclared, which says nothing about the header that
// would have defined them, and every such driver failed for the same one
// reason at a different line.
#include "robohub/platform/stm32_error.h"

/* -------------------------------------------------------------------------- */
/*  Internals                                                                  */
/* -------------------------------------------------------------------------- */

static RoboHubSTM32RS485Context* context_of(RoboHubRS485* self)
{
    if (self == NULL || self->context == NULL) {
        return NULL;
    }
    return (RoboHubSTM32RS485Context*)self->context;
}

static void enable_clock(UART_HandleTypeDef* handle)
{
    if (handle->Instance == USART1) {
        __HAL_RCC_USART1_CLK_ENABLE();
    } else if (handle->Instance == USART2) {
        __HAL_RCC_USART2_CLK_ENABLE();
    } else if (handle->Instance == USART3) {
        __HAL_RCC_USART3_CLK_ENABLE();
    }
#if defined(UART4)
    else if (handle->Instance == UART4) {
        __HAL_RCC_UART4_CLK_ENABLE();
    }
#endif
#if defined(UART5)
    else if (handle->Instance == UART5) {
        __HAL_RCC_UART5_CLK_ENABLE();
    }
#endif
#if defined(USART6)
    else if (handle->Instance == USART6) {
        __HAL_RCC_USART6_CLK_ENABLE();
    }
#endif
}

/* Drive the bus in one direction, in software. */
static RoboHubError drive(RoboHubSTM32RS485Context* ctx, rh_u32 direction)
{
    if (ctx->driver_enable == NULL || ctx->driver_enable->write == NULL) {
        return ROBOHUB_OK;
    }
    rh_u32 level = (direction == RH_RS485_TX) ? ctx->enable_level_for_tx
                                               : (ctx->enable_level_for_tx ? 0u : 1u);
    return ctx->driver_enable->write(ctx->driver_enable,
                                     ctx->driver_enable_pin,
                                     level);
}

/* -------------------------------------------------------------------------- */
/*  Interface operations                                                       */
/* -------------------------------------------------------------------------- */

static RoboHubError op_init(RoboHubRS485* self, rh_u32 baudrate)
{
    RoboHubSTM32RS485Context* ctx = context_of(self);
    if (ctx == NULL || ctx->uart.handle == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (baudrate == 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    enable_clock(ctx->uart.handle);

    ctx->uart.handle->Init.BaudRate = baudrate;
    ctx->uart.handle->Init.WordLength = UART_WORDLENGTH_8B;
    ctx->uart.handle->Init.StopBits = UART_STOPBITS_1;
    ctx->uart.handle->Init.Parity = UART_PARITY_NONE;
    /* Transmit and receive, even though only one is ever live: the peripheral
     * has to be able to receive, and RS485 is half duplex at the transceiver
     * rather than at the UART. */
    ctx->uart.handle->Init.Mode = UART_MODE_TX_RX;
    ctx->uart.handle->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    ctx->uart.handle->Init.OverSampling = UART_OVERSAMPLING_16;

    if (ctx->driver_enable != NULL) {
        /* No DE pin of its own, so the driver drives it: software turnaround. */
        ctx->uart.handle->Init.Mode = UART_MODE_TX_RX;
        ctx->software_direction = 1;
    }

    if (HAL_UART_Init(ctx->uart.handle) != HAL_OK) {
        return robohub_stm32_to_error(HAL_UART_Init(ctx->uart.handle));
    }

    /* Start listening. A bus left transmitting is a bus nobody else can use,
     * and a board that has just been initialised has said nothing yet. */
    RoboHubError err = drive(ctx, RH_RS485_RX);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    ctx->last_direction = RH_RS485_RX;
    ctx->uart.baudrate = baudrate;
    return ROBOHUB_OK;
}

static RoboHubError op_write(RoboHubRS485* self, const rh_u8* data, rh_size len)
{
    RoboHubSTM32RS485Context* ctx = context_of(self);
    if (ctx == NULL || ctx->uart.handle == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (data == NULL && len != 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (ctx->uart.baudrate == 0) {
        return ROBOHUB_ERR_STATE;
    }

    RoboHubError err = drive(ctx, RH_RS485_TX);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }
    ctx->last_direction = RH_RS485_TX;

    rh_u32 timeout = ctx->uart.timeout_ms ? ctx->uart.timeout_ms
                                          : RH_STM32_RS485_DEFAULT_TIMEOUT_MS;
    const rh_u8* cursor = data;
    rh_size remaining = len;
    while (remaining > 0) {
        rh_u16 chunk = (remaining > 0xFFFFu) ? 0xFFFFu : (rh_u16)remaining;
        HAL_StatusTypeDef status = HAL_UART_Transmit(ctx->uart.handle,
                                                      (uint8_t*)cursor,
                                                      chunk,
                                                      timeout);
        if (status != HAL_OK) {
            /* Back to listening even on failure. A write that timed out with
             * the transceiver still driving is a bus left off the network, and
             * the caller is about to be told the write failed and will not be
             * expecting the bus to be wedged as well. */
            (void)drive(ctx, RH_RS485_RX);
            ctx->last_direction = RH_RS485_RX;
            return robohub_stm32_to_error(status);
        }
        cursor += chunk;
        remaining -= chunk;
    }

    err = drive(ctx, RH_RS485_RX);
    ctx->last_direction = RH_RS485_RX;
    return err;
}

static RoboHubError op_read(RoboHubRS485* self, rh_u8* data, rh_size len)
{
    RoboHubSTM32RS485Context* ctx = context_of(self);
    if (ctx == NULL || ctx->uart.handle == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (data == NULL && len != 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return ROBOHUB_OK;
    }
    if (ctx->uart.baudrate == 0) {
        return ROBOHUB_ERR_STATE;
    }

    /* Listening is the state a read is for, and a caller that read after
     * writing has already been put back into it — but a caller that set the
     * direction itself and then read has not, so it is set here rather than
     * assumed. */
    if (ctx->last_direction != RH_RS485_RX) {
        RoboHubError err = drive(ctx, RH_RS485_RX);
        if (ROBOHUB_FAILED(err)) {
            return err;
        }
        ctx->last_direction = RH_RS485_RX;
    }

    rh_u32 timeout = ctx->uart.timeout_ms ? ctx->uart.timeout_ms
                                          : RH_STM32_RS485_DEFAULT_TIMEOUT_MS;
    rh_u8* cursor = data;
    rh_size remaining = len;
    while (remaining > 0) {
        rh_u16 chunk = (remaining > 0xFFFFu) ? 0xFFFFu : (rh_u16)remaining;
        HAL_StatusTypeDef status = HAL_UART_Receive(ctx->uart.handle, cursor, chunk, timeout);
        if (status != HAL_OK) {
            return robohub_stm32_to_error(status);
        }
        cursor += chunk;
        remaining -= chunk;
    }
    return ROBOHUB_OK;
}

static RoboHubError op_set_direction(RoboHubRS485* self, rh_u32 dir)
{
    RoboHubSTM32RS485Context* ctx = context_of(self);
    if (ctx == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (dir != RH_RS485_RX && dir != RH_RS485_TX) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    RoboHubError err = drive(ctx, dir);
    if (ROBOHUB_SUCCEEDED(err)) {
        ctx->last_direction = dir;
    }
    return err;
}

/* -------------------------------------------------------------------------- */
/*  Public API                                                                 */
/* -------------------------------------------------------------------------- */

RoboHubError stm32_rs485_attach(RoboHubRS485* self,
                                rh_name name,
                                RoboHubSTM32RS485Context* context,
                                UART_HandleTypeDef* handle,
                                RoboHubGPIO* driver_enable,
                                rh_u32 driver_enable_pin,
                                rh_u32 enable_level_for_tx,
                                rh_u32 timeout_ms)
{
    if (self == NULL || context == NULL || handle == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    *context = (RoboHubSTM32RS485Context){
        .uart = {
            .handle = handle,
            .baudrate = 0,
            .timeout_ms = timeout_ms,
            .rx_buffer = NULL,
            .rx_capacity = 0,
        },
        .driver_enable = driver_enable,
        .driver_enable_pin = driver_enable_pin,
        .enable_level_for_tx = enable_level_for_tx ? 1u : 0u,
        .software_direction = (driver_enable != NULL) ? 1u : 0u,
        .last_direction = RH_RS485_RX,
    };

    self->name = name;
    self->context = (void*)context;
    self->init = op_init;
    self->write = op_write;
    self->read = op_read;
    self->set_direction = op_set_direction;
    return ROBOHUB_OK;
}

#endif /* CONFIG_RH_HAL_RS485 */
