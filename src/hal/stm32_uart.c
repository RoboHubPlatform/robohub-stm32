/**
 * @file stm32_uart.c
 * @brief UART driver for the RoboHub HAL, on the STM32 HAL.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include "robohub/platform/stm32_uart.h"
#include "robohub/platform/stm32_error.h"

/* -------------------------------------------------------------------------- */
/*  Internals                                                                  */
/* -------------------------------------------------------------------------- */

static RoboHubSTM32UARTContext* context_of(RoboHubUART* self)
{
    if (self == NULL || self->context == NULL) {
        return NULL;
    }
    return (RoboHubSTM32UARTContext*)self->context;
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

/* -------------------------------------------------------------------------- */
/*  Interface operations                                                       */
/* -------------------------------------------------------------------------- */

static RoboHubError op_init(RoboHubUART* self, rh_u32 baudrate)
{
    RoboHubSTM32UARTContext* ctx = context_of(self);
    if (ctx == NULL || ctx->handle == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (baudrate == 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    enable_clock(ctx->handle);

    /* 8N1, no flow control, oversampling 16. This is what a console and most
     * sensors speak, and a board that needed something else would have a board
     * layer setting it rather than a caller at the port. */
    ctx->handle->Init.BaudRate = baudrate;
    ctx->handle->Init.WordLength = UART_WORDLENGTH_8B;
    ctx->handle->Init.StopBits = UART_STOPBITS_1;
    ctx->handle->Init.Parity = UART_PARITY_NONE;
    ctx->handle->Init.Mode = UART_MODE_TX_RX;
    ctx->handle->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    ctx->handle->Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(ctx->handle) != HAL_OK) {
        return robohub_stm32_to_error(HAL_UART_Init(ctx->handle));
    }

    ctx->baudrate = baudrate;
    return ROBOHUB_OK;
}

static RoboHubError op_write(RoboHubUART* self, const rh_u8* data, rh_size len)
{
    RoboHubSTM32UARTContext* ctx = context_of(self);
    if (ctx == NULL || ctx->handle == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (data == NULL && len != 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return ROBOHUB_OK;
    }
    if (ctx->baudrate == 0) {
        return ROBOHUB_ERR_STATE;
    }

    rh_u32 timeout = ctx->timeout_ms ? ctx->timeout_ms : RH_STM32_UART_DEFAULT_TIMEOUT_MS;
    /* A transfer longer than a 16-bit count is split rather than truncated: a
     * caller that asked for 70000 bytes and got 4464, with no error, is worse
     * than one that gets the first 4464 and is told the transfer failed. */
    const rh_u8* cursor = data;
    rh_size remaining = len;
    while (remaining > 0) {
        rh_u16 chunk = (remaining > 0xFFFFu) ? 0xFFFFu : (rh_u16)remaining;
        HAL_StatusTypeDef status = HAL_UART_Transmit(ctx->handle,
                                                      (uint8_t*)cursor,
                                                      chunk,
                                                      timeout);
        if (status != HAL_OK) {
            return robohub_stm32_to_error(status);
        }
        cursor += chunk;
        remaining -= chunk;
    }
    return ROBOHUB_OK;
}

static RoboHubError op_read(RoboHubUART* self, rh_u8* data, rh_size len)
{
    RoboHubSTM32UARTContext* ctx = context_of(self);
    if (ctx == NULL || ctx->handle == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (data == NULL && len != 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return ROBOHUB_OK;
    }
    if (ctx->baudrate == 0) {
        return ROBOHUB_ERR_STATE;
    }

    rh_u32 timeout = ctx->timeout_ms ? ctx->timeout_ms : RH_STM32_UART_DEFAULT_TIMEOUT_MS;
    rh_u8* cursor = data;
    rh_size remaining = len;
    while (remaining > 0) {
        rh_u16 chunk = (remaining > 0xFFFFu) ? 0xFFFFu : (rh_u16)remaining;
        HAL_StatusTypeDef status = HAL_UART_Receive(ctx->handle, cursor, chunk, timeout);
        if (status != HAL_OK) {
            return robohub_stm32_to_error(status);
        }
        cursor += chunk;
        remaining -= chunk;
    }
    return ROBOHUB_OK;
}

/* -------------------------------------------------------------------------- */
/*  Public API                                                                 */
/* -------------------------------------------------------------------------- */

RoboHubError stm32_uart_attach(RoboHubUART* self,
                               rh_name name,
                               RoboHubSTM32UARTContext* context,
                               UART_HandleTypeDef* handle,
                               rh_u32 timeout_ms)
{
    if (self == NULL || context == NULL || handle == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }

    *context = (RoboHubSTM32UARTContext){
        .handle = handle,
        .baudrate = 0,
        .timeout_ms = timeout_ms,
        .rx_buffer = NULL,
        .rx_capacity = 0,
    };

    self->name = name;
    self->context = (void*)context;
    self->init = op_init;
    self->write = op_write;
    self->read = op_read;
    return ROBOHUB_OK;
}

RoboHubError stm32_uart_set_rx_buffer(RoboHubUART* self, rh_u8* buffer, rh_size capacity)
{
    RoboHubSTM32UARTContext* ctx = context_of(self);
    if (ctx == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (buffer == NULL && capacity != 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    ctx->rx_buffer = buffer;
    ctx->rx_capacity = capacity;
    return ROBOHUB_OK;
}

rh_size stm32_uart_available(RoboHubUART* self)
{
    RoboHubSTM32UARTContext* ctx = context_of(self);
    if (ctx == NULL || ctx->handle == NULL) {
        return 0;
    }
    return (rh_size)__HAL_UART_GET_FLAG(ctx->handle, UART_FLAG_RXNE) ? 1u : 0u;
}
