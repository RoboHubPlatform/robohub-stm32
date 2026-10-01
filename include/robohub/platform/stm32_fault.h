/**
 * @file stm32_fault.h
 * @brief What the core says when it could not do what it was told.
 *
 * A fault handler that only spins is a fault handler that throws away the only
 * evidence of what went wrong. The core has already worked it out: it stacked the
 * registers as they were at the faulting instruction, and it set bits in the
 * fault status registers saying what kind of fault it was. All of that is on the
 * stack and in the System Control Block, and it stays there until something
 * reads it.
 *
 * So this module does two things, in this order, and the order is the point.
 * First it makes sure the core will actually *take* the precise fault instead of
 * escalating it. Then it reports what the precise fault was.
 *
 * The escalation is the reason a board usually tells you nothing. At reset the
 * MemManage, BusFault and UsageFault enables in `SHCSR` are clear, so a BusFault
 * does not enter `BusFault_Handler` — it is escalated to `HardFault`, which
 * hardwires `HFSR.FORCED` and stacks a frame describing the *escalation*. The
 * registers still hold the original fault's bits, but the stacked program counter
 * points at the escalation, not at the instruction that faulted, and a report
 * built from that frame names the wrong instruction every time.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
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

/* -------------------------------------------------------------------------- */
/*  The report                                                                 */
/* -------------------------------------------------------------------------- */

/** How many words a report holds: the stacked frame the core pushed, then the
 *  status registers that describe it. A fixed count rather than a growing list,
 *  because this is written to read what a fault left behind and must not need to
 *  allocate to do it. */
#define RH_STM32_FAULT_FRAMES 16u

/**
 * @brief What the core recorded when it faulted.
 *
 * `frame[0..7]` is the stacked exception frame in the order the architecture
 * defines it — r0, r1, r2, r3, r12, lr, pc, xpsr — so `frame[6]` is the address
 * of the instruction that faulted and `frame[5]` is the address it was called
 * from. The words after that are `CFSR`, `HFSR`, `MMFAR`, `BFAR`, `AFSR`,
 * `SHCSR`, `MSP` and `PSP`.
 *
 * `valid` is zero when the core had not faulted, or faulted before this was
 * called, in which case the rest of the structure says nothing.
 */
typedef struct {
    uint32_t valid;
    uint32_t frame[RH_STM32_FAULT_FRAMES];
} RoboHubFaultReport;

/**
 * @brief The last fault this core recorded, kept in RAM.
 *
 * Written by every handler here before it does anything else, so it survives
 * whatever the board does afterwards — including a second fault, and including a
 * fault that happened before the console existed and so could not be printed.
 * A debugger reads it from here; a console reads what the handlers print.
 */
extern volatile RoboHubFaultReport robohub_stm32_last_fault;

/* -------------------------------------------------------------------------- */
/*  The fault handlers                                                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief Take the precise faults instead of escalating them.
 *
 * Enables the MemManage, BusFault and UsageFault handlers in `SHCSR`, so that
 * each is entered in its own right with a frame describing the faulting
 * instruction. Call this before anything that can fault.
 *
 * `HardFault_Handler` remains the last resort for the faults that cannot be
 * enabled — and a fault that arrives with no precise handler still escalates to
 * it, which is why it reports rather than stops.
 */
void robohub_stm32_fault_catch_enable(void);

/**
 * @brief Read the fault the core recorded.
 *
 * Reads the stacked frame and the status registers without clearing them, so a
 * debugger attached afterwards still sees what happened. Safe to call at any
 * time: with no fault recorded it reports `valid == 0`.
 *
 * `exc_return` is the exception return value the handler was entered with — the
 * magic value the architecture leaves in `LR` on exception entry, which is the
 * only thing that says whether the frame is on the main stack or the process
 * one. A caller that is not a handler passes 0, and gets the status registers
 * without a stacked frame.
 */
void robohub_stm32_fault_report(uint32_t exc_return, RoboHubFaultReport* report);

/**
 * @brief Write a fault report to the console.
 *
 * Uses the console driver directly rather than `printf`, and hand-formats
 * rather than calling into the C library. A fault that happened inside the C
 * library's formatted-output path would otherwise fault again here, at the same
 * place, and the second fault would overwrite the evidence of the first.
 */
void robohub_stm32_fault_print(const RoboHubFaultReport* report);

/**
 * @brief Write a fault report to the console and stop.
 *
 * What every handler here ends in. A board that has said what went wrong and
 * then stopped is worth more than one that restarted and faulted again with the
 * register state overwritten, because the second fault destroys the first's
 * evidence.
 */
void robohub_stm32_fault_stop(void);

#ifdef __cplusplus
}
#endif