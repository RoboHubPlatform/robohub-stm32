/**
 * @file stm32_platform.c
 * @brief The RoboHub platform contract on STM32.
 *
 * This is the one function a program calls before anything else, and it is where
 * every piece of hardware knowledge lives: the clock tree, the console UART, the
 * board's own pin map. A program that called the vendor's HAL directly would
 * carry its own copy of all of it, and would only run on the part it was written
 * against.
 *
 * The board's facts are here too, and that is deliberate. Which pin the user LED
 * is on is a fact about the board and not about the family, so it belongs in the
 * board's own support package rather than in a driver; but a driver that has to
 * be told the pin on every call is a driver a program has to know about, so the
 * board registers its pins once here and the program asks for a name.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include "platform.h"
#include "registry.h"
#include "console.h"
#include "error.h"
#include "robohub/platform/stm32_gpio.h"
#include "robohub/platform/stm32_timer.h"
#include "bsp.h"
#include "robohub/platform/stm32_hal.h"

/* -------------------------------------------------------------------------- */
/*  The clock                                                                  */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*  The platform                                                               */
/* -------------------------------------------------------------------------- */

/*
 * The family clock, brought up on the board's behalf.
 *
 * It is not a registry function and it is not the board's: the clock tree is a
 * fact about the family and the drivers, while which pins the console and the LEDs
 * are on are facts about the board. The board's support package calls this,
 * because the board is what knows it is being built for a board.
 */

/* -------------------------------------------------------------------------- */
/*  The clock tree                                                             */
/* -------------------------------------------------------------------------- */

/*
 * This follows ST's own sequence for a Nucleo-144 part, which is the one that is
 * known to boot on this hardware:
 *
 *   1. the MPU, before anything else. Its background region makes the address
 *      space strongly ordered where nothing is defined, and a fault on an
 *      address nobody configured is then a fault with an address in it.
 *   2. HAL_Init, which starts the tick.
 *   3. the supply: wait for the regulator, set the voltage scaling, wait again.
 *   4. the oscillator, then the PLL, then the bus dividers, then the flash
 *      latency that goes with the frequency they reached.
 *
 * Three of those are not what the older families need, and the differences are
 * not cosmetic:
 *
 *   - the Nucleo-144 boards have no crystal on OSC_IN. The ST-LINK's clock
 *     output feeds it, which the HAL calls a *bypass* clock, and a PLL ranged
 *     for a crystal is ranged for the wrong input.
 *   - the H7's PLL has a VCO range and a VCO selector that must be set, and
 *     leaving them at reset gives a VCO the dividers were not chosen for.
 *   - the H7's clock struct names the domain dividers D1PCLK1 and friends, and
 *     the AHB runs at half the core clock on this part.
 */
/*
 * The MPU's background region: everywhere, strongly ordered, no access by
 * default, and no execution.
 *
 * It is the region the hardware consults for any address a later region does not
 * cover, so it is what decides what happens on an address nobody configured.
 */
static void mpu_config(void)
{
    MPU_Region_InitTypeDef region = {0};

    HAL_MPU_Disable();

    region.Enable = MPU_REGION_ENABLE;
    region.BaseAddress = 0x00;
    region.Size = MPU_REGION_SIZE_4GB;
    region.AccessPermission = MPU_REGION_NO_ACCESS;
    region.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    region.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    region.IsShareable = MPU_ACCESS_SHAREABLE;
    region.Number = MPU_REGION_NUMBER0;
    region.TypeExtField = MPU_TEX_LEVEL0;
    region.SubRegionDisable = 0x87;
    region.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;

    HAL_MPU_ConfigRegion(&region);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

#if defined(ROBOHUB_STM32_FAMILY_H7)
static void system_clock_config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clocks = {0};

    /* The regulator, and then the scaling, each waited for. A scaling change
     * that is not complete before the clock is raised runs the core faster than
     * the supply can hold, which is a fault that looks like a software one. */
    while ((PWR->CSR1 & PWR_CSR1_ACTVOSRDY) == 0U) {
    }
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
    }

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    /* Bypass, not a crystal: a Nucleo-144 board takes the ST-LINK's clock on
     * OSC_IN. Naming it as a crystal is a PLL ranged for an input the board is
     * not driving. */
    osc.HSEState = RCC_HSE_BYPASS;
    osc.HSIState = RCC_HSI_OFF;
    osc.CSIState = RCC_CSI_OFF;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 4;
    osc.PLL.PLLN = 400;
    osc.PLL.PLLFRACN = 0;
    osc.PLL.PLLP = 2;
    osc.PLL.PLLR = 2;
    osc.PLL.PLLQ = 4;
    /* The VCO range and selector are set, not left at reset: the dividers above
     * were chosen for a particular VCO, and a VCO in a different range is
     * divided differently. */
    osc.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    osc.PLL.PLLRGE = RCC_PLL1VCIRANGE_1;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        return;  /* the reset clock keeps running; the program says which */
    }

    clocks.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                       RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 |
                       RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1;
    clocks.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clocks.SYSCLKDivider = RCC_SYSCLK_DIV1;
    clocks.AHBCLKDivider = RCC_HCLK_DIV2;
    clocks.APB3CLKDivider = RCC_APB3_DIV2;
    clocks.APB1CLKDivider = RCC_APB1_DIV2;
    clocks.APB2CLKDivider = RCC_APB2_DIV2;
    clocks.APB4CLKDivider = RCC_APB4_DIV2;

    (void)HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_4);
}
#else
static void system_clock_config(void)
{
    /* A family this platform has no verified sequence for. The reset clock
     * keeps running and the program says so, rather than a configuration copied
     * from another family running the core at a frequency nobody asked for. */
}
#endif

void stm32_platform_clock_config(void)
{
    /* The MPU first, then the HAL, then the clock — the vendor's order, and
     * the order is the point. HAL_Init programs the flash and starts the tick,
     * and both of those are memory accesses made before there is an MPU to say
     * what memory means. */
    mpu_config();

    HAL_Init();
    system_clock_config();
}

