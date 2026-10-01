/**
 * @file stm32_console.c
 * @brief The console contract on STM32, and the C library retarget.
 *
 * Two things live here because they are the same wiring seen from two sides. The
 * board's console UART is one peripheral; the console contract drives it, and
 * the C library's `write` is retargeted onto the same driver. Splitting them
 * would mean two places that know which UART the console is, and they would
 * disagree the first time a board was wired differently.
 *
 * The console UART is USART3 on PD8/PD9, which on every Nucleo-144 board is
 * wired to the ST-LINK's virtual COM port. That is the only way to see a
 * program's output without a probe on the pins, and it is why this is the
 * console rather than whichever peripheral happened to be free.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include "robohub/platform/stm32_console.h"
#include "bsp.h"

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <unistd.h>

#include "robohub/platform/stm32_hal.h"


RoboHubConsole robohub_console;

/* The console UART. 115200 8N1, which is what the ST-LINK's virtual COM port
 * presents on every board; a rate the board does not use would show as nothing
 * at all rather than as rubbish. */
#define CONSOLE_BAUD 115200u
#define CONSOLE_TX_TIMEOUT 1000u

static UART_HandleTypeDef console_uart;

/* -------------------------------------------------------------------------- */
/*  The console                                                                */
/* -------------------------------------------------------------------------- */

RoboHubConsoleError robohub_console_init(void)
{
    if (robohub_console.ready) {
        return ROBOHUB_CONSOLE_OK;
    }

    /* The pins and the rate are the board's, not this driver's: which UART is the
     * console and how fast it runs depend on where it is wired and what is at the
     * other end. A driver that decided them would work on one board and fail on
     * every other, by coming up at a rate nothing was listening at. */
    /* The peripheral's clock is enabled by the board, which is what knows
     * which peripheral the console is. Enabling one here would leave a board
     * whose console is a different UART driving an unused clock and starving the
     * one it is using. */

    GPIO_InitTypeDef pins = {0};
    pins.Pin = BSP_CONSOLE_TX_PIN | BSP_CONSOLE_RX_PIN;
    pins.Mode = GPIO_MODE_AF_PP;
    pins.Pull = GPIO_NOPULL;
    pins.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    pins.Alternate = BSP_CONSOLE_AF;
    HAL_GPIO_Init(GPIOD, &pins);

    console_uart.Instance = BSP_CONSOLE_INSTANCE;
    console_uart.Init.BaudRate = BSP_CONSOLE_BAUD;
    console_uart.Init.WordLength = UART_WORDLENGTH_8B;
    console_uart.Init.StopBits = UART_STOPBITS_1;
    console_uart.Init.Parity = UART_PARITY_NONE;
    console_uart.Init.Mode = UART_MODE_TX_RX;
    console_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    console_uart.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&console_uart) != HAL_OK) {
        return ROBOHUB_CONSOLE_ERR_STATE;
    }

    robohub_console.ready = 1u;
    robohub_console.written = 0u;
    return ROBOHUB_CONSOLE_OK;
}

RoboHubConsoleError robohub_console_write(const char* data, size_t len)
{
    if (data == NULL) {
        return ROBOHUB_CONSOLE_ERR_INVALID;
    }
    if (len == 0u) {
        return ROBOHUB_CONSOLE_OK;
    }
    if (!robohub_console.ready) {
        return ROBOHUB_CONSOLE_ERR_STATE;
    }

    /* Written a byte at a time rather than in one call because the transmit
     * data register holds one byte, and a length that does not fit in it has to
     * be fed as the UART frees each. */
    for (size_t i = 0u; i < len; i++) {
        if (HAL_UART_Transmit(&console_uart, (uint8_t*)&data[i], 1u, CONSOLE_TX_TIMEOUT) != HAL_OK) {
            return ROBOHUB_CONSOLE_ERR_TIMEOUT;
        }
    }
    robohub_console.written += (uint64_t)len;
    return ROBOHUB_CONSOLE_OK;
}

RoboHubConsoleError robohub_console_puts(const char* text)
{
    if (text == NULL) {
        return ROBOHUB_CONSOLE_ERR_INVALID;
    }
    size_t length = 0u;
    while (text[length] != '\0') {
        length++;
    }
    return robohub_console_write(text, length);
}

/* -------------------------------------------------------------------------- */
/*  The C library                                                              */
/* -------------------------------------------------------------------------- */

/*
 * Everything below is the C library being taught that this is a board and not a
 * host. On a host, `write` is a kernel call. Here it is a UART, and the
 * functions the C library expects to exist — the ones `errno`, `exit` and the
 * buffered streams call — are provided as failing stubs.
 *
 * The stubs fail rather than succeed, and that is the point: a program that
 * tries to open a file on a board should be told there are no files, because a
 * stub that returned success would have the program believing it had.
 */

/*
 * `fputc`, not `_write`.
 *
 * This is the hook the C library actually calls. `printf` walks its output
 * through the stdio putc path, and the path ends at `fputc`; `_write` is only
 * reached when a *buffered* stream is flushed, which a line-buffered console on a
 * board never is. A retarget that provides only `_write` compiles, links, and
 * prints nothing at all — which is what the vendor's own examples do, and why
 * they are the reference to read rather than to guess at.
 */
int fputc(int ch, FILE* stream)
{
    (void)stream;  /* one console: a program that opens a file on a board gets
                    * the error from _open, not this stream. */
    uint8_t byte = (uint8_t)ch;
    if (robohub_console_write((const char*)&byte, 1u) != ROBOHUB_CONSOLE_OK) {
        return EOF;
    }
    return ch;
}

int _write(int file, char* ptr, int len)
{
    /* Only stdout and stderr reach the console. A program that writes to a
     * file descriptor it made up gets the error it would get on a host. */
    if (file != STDOUT_FILENO && file != STDERR_FILENO) {
        errno = EBADF;
        return -1;
    }
    if (robohub_console_write(ptr, (size_t)len) != ROBOHUB_CONSOLE_OK) {
        errno = EIO;
        return -1;
    }
    return len;
}

int _read(int file, char* ptr, int len)
{
    /* There is no input. The console is a board's way of saying things, not of
     * receiving them; a program that reads is told so rather than blocking for
     * a byte that will never arrive. */
    (void)ptr;
    (void)len;
    if (file != STDIN_FILENO) {
        errno = EBADF;
        return -1;
    }
    return 0;
}

int _close(int file)
{
    (void)file;
    return -1;
}

int _fstat(int file, struct stat* st)
{
    (void)file;
    /* A character device: what a terminal looks like to a program that asks,
     * and what keeps buffered output from deciding it is a file. */
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

void _exit(int status)
{
    (void)status;
    /* There is nowhere to exit to. A program that returns from `main` is how a
     * program ends on a board, and this is only reached if something calls it
     * directly. */
    while (1) {
    }
}

int _times(struct tms* buf)
{
    (void)buf;
    return -1;
}

int _open(const char* path, int flags, int mode)
{
    (void)path;
    (void)flags;
    (void)mode;
    /* No files on a board. */
    return -1;
}

int _unlink(const char* path)
{
    (void)path;
    return -1;
}

int _wait(int* status)
{
    (void)status;
    errno = ECHILD;
    return -1;
}
