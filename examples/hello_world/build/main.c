/* Rho → C backend: module main */

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#define RHO_FEATURE_MATH 1

static char* rho_concat(const char* a, const char* b);
static char* rho_yaml_load(const char* path);

static char* rho_string_from_i32(int32_t value);

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

int main();
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

int main() {
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

static char* rho_concat(const char* a, const char* b) {
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    char* out = malloc(len_a + len_b + 1);
    if (!out) return NULL;
    memcpy(out, a, len_a);
    memcpy(out + len_a, b, len_b + 1);
    return out;
}

static const char* rho_int_to_str(uint32_t n) {
    char buf[16];
    int len = snprintf(buf, sizeof(buf), "%u", n);
    char* out = (char*)malloc(len + 1);
    if (!out) return "";
    memcpy(out, buf, len);
    out[len] = 0;
    return out;
}

static const char* rho_str_from_byte(uint8_t b) {
    char* out = (char*)malloc(2);
    if (!out) return "";
    out[0] = (char)b;
    out[1] = 0;
    return out;
}

static uint32_t rho_int_from_str(const char* s) {
    if (!s || !*s) return 0;
    uint32_t total = 0;
    for (const char* p = s; *p; p++) {
        if (*p < '0' || *p > '9') return 0;
        total = total * 10 + (uint32_t)(*p - '0');
    }
    return total;
}

static bool rho_str_eq(const char* a, const char* b) {
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static void panicf(const char* msg) {
    fprintf(stderr, "rho: %s\n", msg ? msg : "panic");
    exit(1);
}

static uint32_t rho_strlen(const char* s) {
    return s ? (uint32_t)strlen(s) : 0;
}

static bool rho_str_starts_with(const char* s, const char* prefix) {
    if (!s || !prefix) return false;
    size_t n = strlen(prefix);
    return strncmp(s, prefix, n) == 0;
}

static const char* rho_str_trim_prefix(const char* s, const char* prefix) {
    if (!s || !prefix) return s;
    size_t n = strlen(prefix);
    return strncmp(s, prefix, n) == 0 ? s + n : s;
}

static bool rho_str_contains(const char* haystack, const char* needle) {
    if (!haystack || !needle) return false;
    return strstr(haystack, needle) != NULL;
}

static const char* rho_env_or(const char* name, const char* fallback) {
    const char* value = name ? getenv(name) : NULL;
    return value ? value : fallback;
}

static uint8_t rho_str_at(const char* s, uint32_t i) {
    if (!s) return 0;
    size_t n = strlen(s);
    return i < n ? (uint8_t)s[i] : 0;
}

static int32_t rho_str_index_of(const char* haystack, const char* needle) {
    if (!haystack || !needle) return -1;
    const char* found = strstr(haystack, needle);
    return found ? (int32_t)(found - haystack) : -1;
}

static const char* rho_str_slice(const char* s, uint32_t start, uint32_t length) {
    if (!s) return "";
    size_t n = strlen(s);
    if (start >= n) return "";
    size_t avail = n - start;
    size_t take = length < avail ? length : avail;
    char* out = (char*)malloc(take + 1);
    if (!out) return "";
    memcpy(out, s + start, take);
    out[take] = 0;
    return out;
}

static char* rho_yaml_load(const char* path) {
    FILE* f = fopen(path, "r");
    char line[4096];
    char* result = NULL;
    int in_user = 0;
    if (!f) return strdup("unknown");
    while (fgets(line, sizeof(line), f) != NULL) {
        char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t' || *trimmed == '\n' || *trimmed == '\r') trimmed++;
        if (*trimmed == '\0' || *trimmed == '#') continue;
        if (strncmp(trimmed, "user:", 5) == 0) { in_user = 1; continue; }
        if (in_user && strncmp(trimmed, "name:", 5) == 0) {
            char* value = trimmed + 5;
            while (*value == ' ' || *value == '\t') value++;
            size_t len = strlen(value);
            while (len > 0 && (value[len - 1] == '\n' || value[len - 1] == '\r')) { value[--len] = '\0'; }
            result = strdup(value);
            break;
        }
    }
    fclose(f);
    if (result != NULL) return result;
    return strdup("unknown");
}

static char* rho_string_from_i32(int32_t value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    return strdup(buffer);
}

