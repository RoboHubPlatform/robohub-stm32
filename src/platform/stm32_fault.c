/**
 * @file stm32_fault.c
 * @brief Reporting what the core said when it faulted.
 *
 * Four handlers, one body. `MemManage_Handler`, `BusFault_Handler` and
 * `UsageFault_Handler` are entered by the core for the fault they are named
 * after; `HardFault_Handler` is what a fault arrives at when none of them can
 * take it. They share a body because the evidence is the same in all four cases
 * and four copies of it would eventually disagree.
 *
 * Two things here are not the obvious way, and both are the result of getting
 * them wrong first.
 *
 * The first is the entry. The value that says where the core put the frame — the
 * exception return value in `LR` — exists only on exception entry. A handler's
 * own prologue pushes over it, and the compiler is free to schedule a read in C
 * either side of that push, so a handler written in C reads its own return
 * address and reports a fault with no program counter in it. The entry is
 * therefore assembly, and it reads what only exists at entry and passes it on.
 *
 * The second is that this must not re-enter. A fault inside a fault handler is
 * escalated straight back to `HardFault_Handler`, on a stack that is now lower;
 * two entries deep the return address is gone and the core leaves through one
 * that was never a return address at all. On this board that ends as a branch
 * into erased flash, which from outside looks like a program that has lost track
 * of where it is rather than a program that faulted.
 *
 * What this module deliberately does not do is call `printf`. The C library's
 * formatted output allocates, takes a lock and walks a vtable; a fault inside
 * one of those would fault again inside this handler. Everything below is hex
 * digits into the console driver, and nothing at all when the console is closed.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include "robohub/platform/stm32_fault.h"
#include "robohub/platform/stm32_hal.h"
#include "robohub/platform/stm32_console.h"

/* -------------------------------------------------------------------------- */
/*  Registers                                                                  */
/* -------------------------------------------------------------------------- */

/*
 * Spelled out rather than taken from a CMSIS header.
 *
 * These are the whole of what a fault leaves behind, their offsets are fixed by
 * the architecture, and naming them here means the decoding below reads as the
 * architecture's description rather than as header trivia. They are read through
 * pointers cast from the addresses, so a build with no CMSIS device header still
 * reports faults.
 */
#define RH_SCB_BASE  0xE000ED00u
#define RH_SCB_SHCSR (RH_SCB_BASE + 0x24u) /* System Handler Control and State */
#define RH_SCB_CFSR  (RH_SCB_BASE + 0x28u) /* Configurable Fault Status */
#define RH_SCB_HFSR  (RH_SCB_BASE + 0x2Cu) /* HardFault Status */
#define RH_SCB_MMFAR (RH_SCB_BASE + 0x34u) /* MemManage Fault Address */
#define RH_SCB_BFAR  (RH_SCB_BASE + 0x38u) /* BusFault Address */
#define RH_SCB_AFSR  (RH_SCB_BASE + 0x3Cu) /* Auxiliary Fault Status */

/* The enables that make each fault be entered in its own right instead of
 * escalating. At reset all three are clear, and clear means "escalate to
 * HardFault", not "ignore". */
#define RH_SHCSR_FAULT_ENABLES 0x0000000Bu

/* The T bit of a stacked status: the frame was in Thumb state. Its absence means
 * the branch that got here asked for ARM state, which is the one case where the
 * faulting address is the target of a call through a bad pointer rather than an
 * instruction that misbehaved. */
#define RH_XPSR_T 0x01000000u

/* -------------------------------------------------------------------------- */
/*  The report                                                                 */
/* -------------------------------------------------------------------------- */

/*
 * The last fault this core recorded, left in RAM under a name a debugger can
 * find. It is written before anything is printed, so a fault that happened before
 * the console existed — and a fault whose console write faulted — still leaves
 * the evidence somewhere that can be read.
 */
volatile RoboHubFaultReport robohub_stm32_last_fault;

/* Non-zero while a handler is already running. See the note above. */
static volatile uint32_t rh_in_handler;

/* -------------------------------------------------------------------------- */
/*  Entry                                                                     */
/* -------------------------------------------------------------------------- */

/*
 * `lr` is the exception return value here and nowhere else. It is read as the
 * first instruction, before the branch to C, and handed over in r0. `msp` in r1
 * is where the core put the frame — true on entry whether the fault came from
 * thread mode or from a handler, so there is no stack to choose between and no
 * way to choose wrongly. r2 is moved out of the way because the branch needs it
 * free for its own argument passing.
 */
#define RH_FAULT_ENTRY(handler)                     \
    void handler(void);                             \
    __attribute__((naked, used)) void handler(void) \
    {                                               \
        __asm volatile("mov r0, lr\n\t"            \
                       "mrs r1, msp\n\t"           \
                       "mov r2, r3\n\t"            \
                       "b robohub_stm32_fault_entered\n\t"); \
    }

/* Not static: the branch to it is emitted in assembly, and a branch has no way to
 * reach a symbol the linker believes is local. */
void robohub_stm32_fault_entered(uint32_t exc_return, uint32_t* frame_sp,
                               uint32_t held);

RH_FAULT_ENTRY(HardFault_Handler)
RH_FAULT_ENTRY(MemManage_Handler)
RH_FAULT_ENTRY(BusFault_Handler)
RH_FAULT_ENTRY(UsageFault_Handler)

/* -------------------------------------------------------------------------- */
/*  Reading the evidence                                                       */
/* -------------------------------------------------------------------------- */

/*
 * Fill a report from the stacked frame and the status registers.
 *
 * The registers are read, not cleared, so a debugger attached afterwards still
 * sees the fault for itself. `exc_return` is not used to choose a stack — the
 * frame pointer already says which one — only to tell whether a frame was
 * stacked at all, which it is not when the value is not in the 0xFF...... range.
 */
static void rh_collect(uint32_t* frame_sp, RoboHubFaultReport* report)
{
    for (uint32_t i = 0u; i < RH_STM32_FAULT_FRAMES; i++) {
        report->frame[i] = 0u;
    }

    uint32_t cfsr = *(volatile uint32_t*)RH_SCB_CFSR;
    uint32_t hfsr = *(volatile uint32_t*)RH_SCB_HFSR;
    report->valid = (cfsr != 0u || hfsr != 0u) ? 1u : 0u;

    if (frame_sp != 0) {
        /* The architecture's exception frame, in the order it pushes it. */
        report->frame[0] = frame_sp[0]; /* r0   */
        report->frame[1] = frame_sp[1]; /* r1   */
        report->frame[2] = frame_sp[2]; /* r2   */
        report->frame[3] = frame_sp[3]; /* r3   */
        report->frame[4] = frame_sp[4]; /* r12  */
        report->frame[5] = frame_sp[5]; /* lr   */
        report->frame[6] = frame_sp[6]; /* pc   */
        report->frame[7] = frame_sp[7]; /* xpsr */
    }

    report->frame[8] = cfsr;
    report->frame[9] = hfsr;
    report->frame[10] = *(volatile uint32_t*)RH_SCB_MMFAR;
    report->frame[11] = *(volatile uint32_t*)RH_SCB_BFAR;
    report->frame[12] = *(volatile uint32_t*)RH_SCB_AFSR;
    report->frame[13] = *(volatile uint32_t*)RH_SCB_SHCSR;
    report->frame[14] = __get_MSP();
    report->frame[15] = __get_PSP();
}

void robohub_stm32_fault_report(uint32_t exc_return, RoboHubFaultReport* report)
{
    if (report == 0) {
        return;
    }
    /* An exception return value is always in 0xFF...... . Anything else means the
     * caller is not a fault handler, so there is no stacked frame to point at. */
    rh_collect(((exc_return & 0xFF000000u) == 0xFF000000u) ? (uint32_t*)__get_MSP()
                                                           : 0,
               report);
}

/* -------------------------------------------------------------------------- */
/*  Printing                                                                   */
/* -------------------------------------------------------------------------- */

/*
 * Hex, by hand, into a stack buffer. `printf("%08x")` is the short answer and is
 * wrong here for the reason the whole module exists.
 */
static size_t rh_hex(char* out, size_t at, uint32_t value, uint32_t width)
{
    static const char digits[] = "0123456789abcdef";
    for (uint32_t i = width; i > 0u; i--) {
        out[at++] = digits[(value >> ((i - 1u) * 4u)) & 0xFu];
    }
    return at;
}

static void rh_print(const char* text)
{
    (void)robohub_console_puts(text);
}

static void rh_print_hex(const char* label, uint32_t value)
{
    char line[32];
    size_t at = 0u;
    for (const char* c = label; *c != '\0'; c++) {
        line[at++] = *c;
    }
    line[at++] = '=';
    at = rh_hex(line, at, value, 8u);
    line[at++] = '\n';
    (void)robohub_console_write(line, at);
}

void robohub_stm32_fault_print(const RoboHubFaultReport* report)
{
    if (report == 0) {
        return;
    }

    /* Only once the console is actually open.
     *
     * A fault can arrive before the console exists — during startup, or in a
     * program that never opened one — and the UART handle the driver would
     * transmit through is then whatever was in memory. Driving it faults again,
     * inside this handler, and the second fault is stacked on top of the first's,
     * so the board reports a fault in the fault reporter. That is worse than
     * silence: it points at the diagnosis rather than the fault. */
    if (robohub_console.ready == 0u) {
        return;
    }

    if (report->valid == 0u) {
        rh_print("fault: none recorded\n");
        return;
    }

    rh_print("\n=== fault ===\n");
    /* Where it happened and what it was, first. */
    rh_print_hex("pc     ", report->frame[6]);
    rh_print_hex("lr     ", report->frame[5]);
    rh_print_hex("xpsr   ", report->frame[7]);
    rh_print_hex("cfsr   ", report->frame[8]);
    rh_print_hex("hfsr   ", report->frame[9]);
    rh_print_hex("mmfar  ", report->frame[10]);
    rh_print_hex("bfar   ", report->frame[11]);
    rh_print_hex("afsr   ", report->frame[12]);
    rh_print_hex("shcsr  ", report->frame[13]);
    rh_print_hex("msp    ", report->frame[14]);
    rh_print_hex("psp    ", report->frame[15]);

    /* CFSR decoded, because a raw hex word does not say which of the faults it
     * was — and which one it was is the whole of the diagnosis. */
    uint32_t cfsr = report->frame[8];
    if ((cfsr & 0x00000001u) != 0u) { rh_print("cfsr: IACCVIOL\n"); }
    if ((cfsr & 0x00000002u) != 0u) { rh_print("cfsr: DACCVIOL\n"); }
    if ((cfsr & 0x00000008u) != 0u) { rh_print("cfsr: MUNSTKERR\n"); }
    if ((cfsr & 0x00000010u) != 0u) { rh_print("cfsr: MSTKERR\n"); }
    if ((cfsr & 0x00000020u) != 0u) { rh_print("cfsr: MMARVALID\n"); }
    if ((cfsr & 0x00000100u) != 0u) { rh_print("cfsr: IBUSERR\n"); }
    if ((cfsr & 0x00000200u) != 0u) { rh_print("cfsr: PRECISERR\n"); }
    if ((cfsr & 0x00000400u) != 0u) { rh_print("cfsr: IMPRECISERR\n"); }
    if ((cfsr & 0x00000800u) != 0u) { rh_print("cfsr: UNSTKERR\n"); }
    if ((cfsr & 0x00001000u) != 0u) { rh_print("cfsr: STKERR\n"); }
    if ((cfsr & 0x00002000u) != 0u) { rh_print("cfsr: BFARVALID\n"); }
    if ((cfsr & 0x00010000u) != 0u) { rh_print("cfsr: INVSTATE\n"); }
    if ((cfsr & 0x00020000u) != 0u) { rh_print("cfsr: INVPC\n"); }
    if ((cfsr & 0x00040000u) != 0u) { rh_print("cfsr: NOCP\n"); }
    if ((cfsr & 0x00080000u) != 0u) { rh_print("cfsr: UNALIGNED\n"); }
    if ((cfsr & 0x00100000u) != 0u) { rh_print("cfsr: DIVBYZERO\n"); }

    if ((report->frame[7] & RH_XPSR_T) == 0u) {
        rh_print("xpsr: T clear - reached through an address that was not Thumb\n");
    }
}

/* -------------------------------------------------------------------------- */
/*  Enabling                                                                   */
/* -------------------------------------------------------------------------- */

void robohub_stm32_fault_catch_enable(void)
{
    *(volatile uint32_t*)RH_SCB_SHCSR |= RH_SHCSR_FAULT_ENABLES;
    __DSB();
    __ISB();
}

/* -------------------------------------------------------------------------- */
/*  The body                                                                   */
/* -------------------------------------------------------------------------- */

void robohub_stm32_fault_stop(void)
{
    for (;;) {
        __NOP();
    }
}

void robohub_stm32_fault_entered(uint32_t exc_return, uint32_t* frame_sp,
                               uint32_t held)
{
    (void)exc_return;
    (void)held;

    if (rh_in_handler != 0u) {
        /* Already here once. The fault that got the board here is being handled
         * above this frame; a second one arriving inside it cannot be reported
         * without overwriting the first, so it is dropped and the first handler
         * is left to finish. */
        return;
    }
    rh_in_handler = 1u;

    __disable_irq();

    /* Read straight into the saved report. The frame is the first thing worth
     * having, and building it elsewhere first would need 68 more bytes of stack
     * in the one place on the board with the least of it. */
    RoboHubFaultReport* report =
        (RoboHubFaultReport*)(void*)&robohub_stm32_last_fault;
    rh_collect(frame_sp, report);

    robohub_stm32_fault_print(report);

    robohub_stm32_fault_stop();
}