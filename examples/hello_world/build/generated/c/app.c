/*
 * @file app.c
 * @brief Generated Rho application implementation.
 *
 * @project main
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L
#include "app.h"

/* Rho → C backend: module main */

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#define RHO_FEATURE_MATH 1

char* rho_concat(const char* a, const char* b);
char* rho_yaml_load(const char* path);

char* rho_string_from_i32(int32_t value);

static bool rho_str_eq(const char* a, const char* b);

static void panicf(const char* msg);

static uint32_t rho_strlen(const char* s);

static bool rho_str_starts_with(const char* s, const char* prefix);

static const char* rho_str_trim_prefix(const char* s, const char* prefix);

static bool rho_str_contains(const char* haystack, const char* needle);

static const char* rho_env_or(const char* name, const char* fallback);

static uint8_t rho_str_at(const char* s, uint32_t i);

static int32_t rho_str_index_of(const char* haystack, const char* needle);

static const char* rho_str_slice(const char* s, uint32_t start, uint32_t length);

static const char* rho_int_to_str(uint32_t n);

static const char* rho_str_from_byte(uint8_t b);

static uint32_t rho_int_from_str(const char* s);

typedef struct {
    uint64_t raw;
} Ptr_RoboHubGPIO;

typedef struct {
    uint64_t raw;
} Ptr_RoboHubTimer;

typedef struct {
    uint64_t raw;
} Ptr_u32;

typedef struct {
    uint64_t raw;
} Ptr_u8;

int app_main();
int32_t ffi_robohub_board_register();
int32_t ffi_robohub_console_init();
int32_t ffi_robohub_console_puts(Ptr_u8 text);
int32_t ffi_robohub_console_write(Ptr_u8 data, uint64_t len);
Ptr_u8 ffi_robohub_error_to_string(int32_t err);
int32_t ffi_robohub_platform_init();
int32_t ffi_rho_robohubgpio_init(Ptr_RoboHubGPIO self);
int32_t ffi_rho_robohubgpio_read(Ptr_RoboHubGPIO self, uint32_t pin, Ptr_u32 value);
int32_t ffi_rho_robohubgpio_set_mode(Ptr_RoboHubGPIO self, uint32_t pin, uint32_t mode);
int32_t ffi_rho_robohubgpio_write(Ptr_RoboHubGPIO self, uint32_t pin, uint32_t value);
void ffi_rho_robohubtimer_delay_ms(Ptr_RoboHubTimer self, uint32_t ms);
void ffi_rho_robohubtimer_delay_us(Ptr_RoboHubTimer self, uint32_t us);
uint64_t ffi_rho_robohubtimer_get_micros(Ptr_RoboHubTimer self);
uint64_t ffi_rho_robohubtimer_get_millis(Ptr_RoboHubTimer self);

int app_main() {
    return 0;
}

int32_t ffi_robohub_board_register();

int32_t ffi_robohub_console_init();

int32_t ffi_robohub_console_puts(Ptr_u8 text);

int32_t ffi_robohub_console_write(Ptr_u8 data, uint64_t len);

Ptr_u8 ffi_robohub_error_to_string(int32_t err);

int32_t ffi_robohub_platform_init();

int32_t ffi_rho_robohubgpio_init(Ptr_RoboHubGPIO self);

int32_t ffi_rho_robohubgpio_read(Ptr_RoboHubGPIO self, uint32_t pin, Ptr_u32 value);

int32_t ffi_rho_robohubgpio_set_mode(Ptr_RoboHubGPIO self, uint32_t pin, uint32_t mode);

int32_t ffi_rho_robohubgpio_write(Ptr_RoboHubGPIO self, uint32_t pin, uint32_t value);

void ffi_rho_robohubtimer_delay_ms(Ptr_RoboHubTimer self, uint32_t ms);

void ffi_rho_robohubtimer_delay_us(Ptr_RoboHubTimer self, uint32_t us);

uint64_t ffi_rho_robohubtimer_get_micros(Ptr_RoboHubTimer self);

uint64_t ffi_rho_robohubtimer_get_millis(Ptr_RoboHubTimer self);


void app_init(void)
{
}

