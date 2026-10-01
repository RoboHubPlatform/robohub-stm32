/**
 * @file test_error.c
 * @brief Tests for the STM32 HAL to RoboHub error mapping.
 *
 * The mapping is the one part of this platform that is testable without a
 * board, because it depends on the vendor's published status values rather
 * than on a peripheral. These tests run on the host and are the reason a
 * driver can rely on the mapping being right.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <stdio.h>

#include "error.h"
#include "robohub/platform/stm32_error.h"

static int failures = 0;

static void expect(const char* what, RoboHubError got, RoboHubError want)
{
    if (got == want) {
        return;
    }
    printf("FAIL %s: got %d, want %d\n", what, (int)got, (int)want);
    failures++;
}

/* Every status the HAL defines maps to the code a caller can act on. */
static void test_known_statuses(void)
{
    expect("HAL_OK",     robohub_stm32_to_error(RH_STM32_HAL_OK),      ROBOHUB_OK);
    expect("HAL_ERROR",  robohub_stm32_to_error(RH_STM32_HAL_ERROR),   ROBOHUB_ERR_FAIL);
    expect("HAL_BUSY",   robohub_stm32_to_error(RH_STM32_HAL_BUSY),    ROBOHUB_ERR_BUSY);
    expect("HAL_TIMEOUT", robohub_stm32_to_error(RH_STM32_HAL_TIMEOUT), ROBOHUB_ERR_TIMEOUT);
}

/* A status nobody mapped is a failure, not a success. Reporting it as success
 * is the one wrong answer here, because a caller that is told a transfer
 * worked when it did not has nothing left to check. */
static void test_unmapped_status_is_not_success(void)
{
    expect("unmapped", robohub_stm32_to_error(99), ROBOHUB_ERR_UNKNOWN);
    expect("negative", robohub_stm32_to_error(-7), ROBOHUB_ERR_UNKNOWN);
}

/* The mapper handed to robohub-common agrees with the direct mapping, because
 * a caller holding only a mapper must not see a different answer. */
static void test_mapper_agrees(void)
{
    expect("mapper ok",      robohub_stm32_error_mapper(RH_STM32_HAL_OK),      ROBOHUB_OK);
    expect("mapper timeout", robohub_stm32_error_mapper(RH_STM32_HAL_TIMEOUT), ROBOHUB_ERR_TIMEOUT);
}

int main(void)
{
    test_known_statuses();
    test_unmapped_status_is_not_success();
    test_mapper_agrees();

    if (failures != 0) {
        printf("robohub-stm32 error mapping: %d failure(s)\n", failures);
        return 1;
    }
    printf("robohub-stm32 error mapping: all checks passed\n");
    return 0;
}
