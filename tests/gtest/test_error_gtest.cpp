/**
 * @file test_error_gtest.c
 * @brief GoogleTest suite for the HAL-to-RoboHub error mapping.
 *
 * The mapping is the one part of this platform a host can test, because it depends
 * on the vendor's published status values rather than on a peripheral — which is
 * also why it is worth testing hard. Every other call a program makes into this
 * platform is answered by hardware whose answer it cannot check, and this is the
 * function that decides how that answer is worded.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <gtest/gtest.h>

#include "error.h"
#include "robohub/platform/stm32_error.h"

/* Every status the vendor defines maps to the code a caller can act on. A driver
 * that reports HAL_TIMEOUT as a plain failure has thrown away the distinction
 * between "the peripheral was busy" and "the peripheral never answered", and the
 * caller can no longer choose to retry. */
TEST(ErrorMapping, EveryKnownStatusMapsToACodeACallerCanActOn)
{
    EXPECT_EQ(ROBOHUB_OK,        robohub_stm32_to_error(RH_STM32_HAL_OK));
    EXPECT_EQ(ROBOHUB_ERR_FAIL,  robohub_stm32_to_error(RH_STM32_HAL_ERROR));
    EXPECT_EQ(ROBOHUB_ERR_BUSY,  robohub_stm32_to_error(RH_STM32_HAL_BUSY));
    EXPECT_EQ(ROBOHUB_ERR_TIMEOUT, robohub_stm32_to_error(RH_STM32_HAL_TIMEOUT));
}

/* A status nobody mapped is a failure, not a success.
 *
 * This is the assertion worth arguing for. The alternative — treating an
 * unrecognised non-zero status as success — is the one answer that cannot be
 * recovered from: a caller told a transfer worked when it did not has nothing
 * left to check, and the fault surfaces wherever the data was meant to arrive. */
TEST(ErrorMapping, AnUnmappedStatusIsAFailureRatherThanASuccess)
{
    EXPECT_EQ(ROBOHUB_ERR_UNKNOWN, robohub_stm32_to_error(99));
    EXPECT_EQ(ROBOHUB_ERR_UNKNOWN, robohub_stm32_to_error(-7));
    EXPECT_NE(ROBOHUB_OK, robohub_stm32_to_error(99));
}

/* Success is success and nothing else is, which is the other half of the same
 * claim: a mapping that returned OK for anything would pass the test above. */
TEST(ErrorMapping, OnlyTheVendorsSuccessIsSuccess)
{
    for (int status = 0; status < 16; status++) {
        RoboHubError mapped = robohub_stm32_to_error(status);
        if (status == RH_STM32_HAL_OK) {
            EXPECT_EQ(ROBOHUB_OK, mapped) << "status " << status;
        } else {
            EXPECT_NE(ROBOHUB_OK, mapped) << "status " << status;
        }
    }
}

/* The mapper handed to robohub-common and the direct call must answer alike.
 * A caller holding only a mapper — which is most of them — must not get a
 * different answer from one holding the function. */
TEST(ErrorMapping, TheMapperAgreesWithTheDirectMapping)
{
    for (int status = 0; status < 8; status++) {
        EXPECT_EQ(robohub_stm32_to_error(status), robohub_stm32_error_mapper(status))
            << "status " << status;
    }
}

/* The mapping is a pure function of its argument, which is what lets a caller
 * rely on it before anything has happened. */
TEST(ErrorMapping, TheMappingIsRepeatable)
{
    for (int status = 0; status < 8; status++) {
        RoboHubError first = robohub_stm32_to_error(status);
        EXPECT_EQ(first, robohub_stm32_to_error(status)) << "status " << status;
    }
}