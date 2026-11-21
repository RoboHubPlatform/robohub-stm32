/**
 * @file main.c
 * @brief health_check — the health contracts and the keystore on a Nucleo board
 *        (STM-0008, STM-0009).
 *
 * See module.yml: three boots, a watchdog reset, a recorded fault, a fed
 * watchdog. The watchdog is the core's (CONFIG_RH_CORE_WATCHDOG_MS=3000).
 *
 * The keystore rides the same three boots (STM-0009; the keystore in flash): the first stores key A,
 * the second finds A after the watchdog reset and stores B, the third finds B
 * after the fault reset. The check's keys have their own scope and epoch 0, so
 * no session ever uses them; they do replace a key the board was rotated to,
 * so flash the board from Mesh afterwards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"
#include "registry.h"
#include "types.h"
#include "robohub/hal/console.h"
#include "robohub/hal/timer.h"
#include "robohub/health.h"
#include "robohub/keystore.h"
#include "robohub/core/core.h"
#include "robohub/core/health.h"
#include "robohub/core/scheduler.h"

static void put(const char* text)
{
    (void)robohub_console_puts(text);
}

static void put_u32(uint32_t value)
{
    char digits[12];
    uint32_t count = 0;
    do {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value > 0u);
    char out[12];
    uint32_t at = 0;
    while (count > 0u) {
        out[at++] = digits[--count];
    }
    out[at] = '\0';
    put(out);
}

static void put_hex(uint32_t value)
{
    static const char hex[] = "0123456789abcdef";
    char out[11] = "0x";
    for (int i = 0; i < 8; i++) {
        out[2 + i] = hex[(value >> (28 - 4 * i)) & 0xFu];
    }
    out[10] = '\0';
    put(out);
}

/* The check's keys: scope "rhcheck", epoch 0 (never newer than a compiled key),
 * and a body that says which step wrote it. B is written only when A came back
 * after the reset, so finding B on the third boot means both held. */
static const char check_scope[8] = {'r', 'h', 'c', 'h', 'e', 'c', 'k', 0};

static rh_key_t check_key(uint8_t which)
{
    rh_key_t key;
    key.epoch = 0u;
    for (int i = 0; i < 8; i++) {
        key.scope[i] = (uint8_t)check_scope[i];
    }
    for (int i = 0; i < 16; i++) {
        key.key[i] = (uint8_t)(which ^ (uint8_t)(i * 17));
    }
    return key;
}

static bool same_key(const rh_key_t* a, const rh_key_t* b)
{
    if (a->epoch != b->epoch) {
        return false;
    }
    for (int i = 0; i < 8; i++) {
        if (a->scope[i] != b->scope[i]) {
            return false;
        }
    }
    for (int i = 0; i < 16; i++) {
        if (a->key[i] != b->key[i]) {
            return false;
        }
    }
    return true;
}

/* Whether the stored key is `which`. */
static bool keystore_holds(uint8_t which)
{
    rh_key_t want = check_key(which);
    rh_key_t got;
    return rh_keystore_load(&got) == 0 && same_key(&got, &want);
}

/* Store `which` and read it straight back. */
static bool keystore_put(uint8_t which)
{
    rh_key_t key = check_key(which);
    return rh_keystore_store(&key) == 0 && keystore_holds(which);
}

static bool keystore_ok;

static robohub_core_health health;
static robohub_core_task_id hang_task, fault_task, beat_task;

/* Step 1: after a second, a task stops returning. The loop stops, nothing feeds
 * the watchdog, and the board resets one period later. */
static RoboHubError hang(robohub_core_task_id id, void* user)
{
    (void)id; (void)user;
    if (robohub_core_uptime_ms() < 1000u) {
        return ROBOHUB_OK;
    }
    put("health-check: step 1 — task ");
    put_u32(id);
    put(" hangs now; the watchdog should reset the board within 3 s\n");
    for (volatile uint32_t spin = 0;; spin++) {
    }
    return ROBOHUB_OK;
}

/* Step 2: after a second, a task writes where nothing is. */
static RoboHubError fault(robohub_core_task_id id, void* user)
{
    (void)id; (void)user;
    if (robohub_core_uptime_ms() < 1000u) {
        return ROBOHUB_OK;
    }
    put("health-check: step 2 — task ");
    put_u32(id);
    put(" faults now; the board should record it and restart\n");
    *(volatile uint32_t*)0xF0000000u = 1u;
    return ROBOHUB_OK;
}

/* Step 3: tasks keep their time and the loop keeps the watchdog fed. */
static RoboHubError beat(robohub_core_task_id id, void* user)
{
    (void)id; (void)user;
    robohub_core_health now;
    robohub_core_health_get(&now);
    uint32_t seconds = robohub_core_uptime_ms() / 1000u;
    /* The whole chain again, for a console opened late: boot 3 means both
     * resets kept the retained record. */
    put("health-check: boot ");
    put_u32(health.boot_count);
    put(", reset reason ");
    put(rh_reset_reason_name(health.reset_reason));
    put(", fault code ");
    put_u32(health.fault.code);
    put(" in task ");
    put_u32(health.fault.task);
    put(" at ");
    put_hex(health.fault.pc);
    put("\n");
    put("health-check: alive ");
    put_u32(seconds);
    put(" s, watchdog fed ");
    put_u32(now.watchdog_feeds);
    put(" times\n");
    if (seconds >= 20u) {
        if (!keystore_ok) {
            put("health-check: FAILED — the keystore did not hold its keys across the resets\n");
        } else {
            put(now.watchdog_ms != 0u && now.watchdog_feeds > 100u
                    ? "health-check: passed\n"
                    : "health-check: FAILED — the watchdog was not running or not fed\n");
        }
    }
    return ROBOHUB_OK;
}

int main(void)
{
    if (robohub_platform_init() != ROBOHUB_OK) {
        return 1;
    }
    RoboHubTimer* timer = robohub_get_timer("system_timer");
    /* Time for the ST-LINK's serial console to be opened after a reset. */
    for (int i = 0; i < 30 && timer != NULL; i++) {
        timer->delay_ms(timer, 100u);
    }

    if (robohub_core_init(NULL) != ROBOHUB_OK) {
        put("health-check: FAILED — the core did not start\n");
        return 1;
    }
    robohub_core_health_get(&health);
    put("\nhealth-check: boot ");
    put_u32(health.boot_count);
    put(", reset reason ");
    put(rh_reset_reason_name(health.reset_reason));
    put("\n");
    if (health.has_fault) {
        put("health-check: fault record — code ");
        put_u32(health.fault.code);
        put(", task ");
        put_u32(health.fault.task);
        put(", pc ");
        put_hex(health.fault.pc);
        put(", uptime ");
        put_u32(health.fault.uptime_ms);
        put(" ms\n");
    } else {
        put("health-check: no fault record\n");
    }

    /* On this part a HardFault does not reset the board itself: the handler
     * writes the record and stops, and the running watchdog resets it. So the
     * third boot is known by its record — a CPU fault — and its reason reads
     * watchdog (STM-0008). */
    bool after_fault = health.has_fault && health.fault.code == RH_FAULT_HARD &&
                       (health.reset_reason == RH_RESET_FAULT || health.reset_reason == RH_RESET_WATCHDOG);
    if (after_fault) {
        if (health.fault.pc == 0u || health.fault.task == RH_HEALTH_NO_TASK) {
            put("health-check: FAILED — a fault reset without the record it should have left\n");
        } else {
            put("health-check: step 3 — the fault was recorded; running with the watchdog fed\n");
        }
        keystore_ok = keystore_holds(0xB0u);
        put(keystore_ok ? "health-check: keystore — key B held across the fault reset\n"
                        : "health-check: keystore — key B is not there after the fault reset\n");
        (void)robohub_core_task_add("beat", beat, 0, 2000u, &beat_task);
    } else if (health.reset_reason == RH_RESET_WATCHDOG) {
        if (keystore_holds(0xA0u)) {
            put("health-check: keystore — key A held across the watchdog reset\n");
            put(keystore_put(0xB0u) ? "health-check: keystore — key B stored and read back\n"
                                    : "health-check: keystore — key B could not be stored\n");
        } else {
            put("health-check: keystore — key A is not there after the watchdog reset\n");
        }
        (void)robohub_core_task_add("fault", fault, 0, 100u, &fault_task);
    } else {
        put(keystore_put(0xA0u) ? "health-check: keystore — key A stored and read back\n"
                                : "health-check: keystore — key A could not be stored\n");
        (void)robohub_core_task_add("hang", hang, 0, 100u, &hang_task);
    }
    (void)robohub_core_run();
    return 0;
}
