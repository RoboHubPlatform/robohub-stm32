/*
 * @file runtime.c
 * @brief Generated Rho C runtime support.
 *
 * @project main
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

char* rho_concat(const char* a, const char* b) {
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

char* rho_yaml_load(const char* path) {
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

char* rho_string_from_i32(int32_t value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    return strdup(buffer);
}

