/**
 * @file stm32_hal.h
 * @brief The vendor library this platform is built against.
 *
 * Every driver in this module reaches the STM32 HAL through this header and
 * through nothing else, so that the one place the vendor's names appear is one
 * file. A driver that included `stm32h7xx_hal.h` directly would be a driver
 * that only builds for the family whose header it named, and the mistake would
 * surface as a compile error on some other board rather than as a wrong answer.
 *
 * The HAL is included by its own name, and which name that is comes from the
 * build: a target spec for a part says which configuration header that part uses
 * and defines it here. The alternative — a header that picked a name — would be
 * right for one family and wrong for every other, and the wrongness would only
 * appear on the boards nobody was holding at the time.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The build says which part this is, by naming the HAL header for it. A header
 * that picked the name itself would be right for one family and wrong for every
 * other, and the wrongness would appear only on the boards nobody had to hand.
 *
 * The HAL is included before anything else, and not the configuration before it
 * as one might expect. The vendor's headers are mutually recursive —
 * `hal_def.h` includes a legacy header that includes a module header that needs a
 * type `hal_def.h` has not defined yet — and the cycle only closes because the
 * HAL header guards itself on the way in. Including the configuration first
 * reaches the module header before that guard is set, and the build fails inside
 * the vendor's own headers with an error about a type nobody in this project
 * mentioned. */
#if !defined(ROBOHUB_STM32_HAL_HEADER)
#error "This build did not say which STM32 family it is for. A target spec for the part declares ROBOHUB_STM32_HAL_HEADER; nothing here can know it."
#endif

#include ROBOHUB_STM32_HAL_HEADER

/* -------------------------------------------------------------------------- */
/*  One name for a thing the vendor spells differently                          */
/* -------------------------------------------------------------------------- */
//
// ST renamed a good number of HAL symbols between families and kept both
// spellings working in some, neither in others. `GPIO_MODE_OD` became
// `GPIO_MODE_OUTPUT_OD`; `GPIO_MODE_PP` became `GPIO_MODE_OUTPUT_PP`. A driver
// that picked between them had to know which families used which, and the
// knowledge went in the driver as a list of family names.
//
// That is where this comment is really about: a list of family names is a
// promise to be extended every time ST ships a family, and the failure when it
// is not is a driver that compiles against a macro nobody defined, on one
// family, out of twenty. Nobody notices until somebody builds that board.
//
// So the question is asked of the headers instead of of a list. Each name this
// platform uses is defined here in terms of whatever the vendor actually spelled
// it, once, for every family, and a driver uses the one name. A family that
// renames a symbol is a new line here, not a new branch in every driver, and a
// family nobody has heard of still compiles as long as it spells the symbol the
// way the ones above do.
//
// Each is guarded on the absence of the name, so a vendor header that defines
// it first keeps its own. That is the older spelling winning where both exist,
// which is what the vendor means by providing both.

/* Open-drain output: `GPIO_MODE_OUTPUT_OD` since the H7 naming, `GPIO_MODE_OD`
   before it. F0, C0, G0, L0 and L1 use the shorter one. */
#ifndef GPIO_MODE_OUTPUT_OD
    #ifdef GPIO_MODE_OD
        #define GPIO_MODE_OUTPUT_OD GPIO_MODE_OD
    #endif
#endif

/* Push-pull output, the same pair of names. */
#ifndef GPIO_MODE_OUTPUT_PP
    #ifdef GPIO_MODE_PP
        #define GPIO_MODE_OUTPUT_PP GPIO_MODE_PP
    #endif
#endif

#ifdef __cplusplus
}
#endif
