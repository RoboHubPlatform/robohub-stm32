/*
 * @file app.h
 * @brief Generated Rho application interface.
 *
 * @project main
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <platform.h>
#include <console.h>
#include <gpio.h>
#include <timer.h>
#include <error.h>
#include <types.h>
#include "autoconf.h"

int app_main(void);
void app_init(void);

#ifdef __cplusplus
}
#endif
