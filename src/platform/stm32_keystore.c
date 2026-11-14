/**
 * @file stm32_keystore.c
 * @brief The device keystore in the last two flash sectors of bank 2, on the
 *        STM32H7 boards whose memory map reserves them.
 *
 * Which boards: a board opts in by defining RH_KEYSTORE_STM32_FLASH=1 in its
 * target spec AND shortening its `memory.flash.length` by two sectors, so the
 * generated linker script never places code there. Today that is the
 * NUCLEO-H753ZI (2 MB, bank 2 sectors 6 and 7: 0x081C0000-0x081FFFFF, 256 KB).
 * Every other board links the weak default in robohub-common (no store; REKEY is
 * answered "cannot rotate"), for the reasons docs/index.md gives per family:
 *
 *  - STM32F2/F4/F7: the sectors at the end of flash are 128 KB (256 KB on F7),
 *    and the small 16/32 KB ones are at the start, where the vector table is;
 *    using them means relinking the image around a hole, which is not done yet.
 *  - STM32H72x/H73x: single bank, so erasing a sector stalls the code running
 *    from the same bank; and boards whose target spec has no memory map (no
 *    generated linker script) cannot reserve anything.
 *
 * Writing bank 2 while the image runs from bank 1 is read-while-write, so no
 * code stalls; the slot is erased with HAL_FLASHEx_Erase and programmed one
 * flash word (256 or 128 bits) at a time. A 40-byte record takes two flash words
 * on an H74x/H75x and three on an H7Ax/H7Bx, the tail padded with 0xFF. Flash words are never re-programmed (the
 * ECC forbids it), which the A/B scheme guarantees: a slot is always erased
 * before it is written.
 *
 * Before touching flash, the image's end (from the generated linker script's
 * _sidata/_sdata/_edata) is checked against the keystore's start, so a board
 * that opted in without shortening its memory map refuses rather than erasing
 * its own code.
 *
 * SPDX-License-Identifier: MIT
 */

#include "robohub/keystore_core.h"

/* Named on the link (--undefined) so this object, and the strong functions
 * below, are always taken from the platform's archive; see keystore_core.h. */
const char rh_keystore_platform[] = "stm32";

#if defined(RH_KEYSTORE_STM32_FLASH) && RH_KEYSTORE_STM32_FLASH

#include "robohub/platform/stm32_hal.h"

#include <string.h>

#if !defined(FLASH_TYPEPROGRAM_FLASHWORD) || !defined(DUAL_BANK) || !defined(FLASH_BANK2_BASE)
#error "RH_KEYSTORE_STM32_FLASH is implemented for dual-bank STM32H7 parts only; leave it undefined for this board"
#endif

#define RH_KEYSTORE_FIRST_SECTOR (FLASH_SECTOR_TOTAL - 2u)
#define RH_KEYSTORE_BASE (FLASH_BANK2_BASE + RH_KEYSTORE_FIRST_SECTOR * FLASH_SECTOR_SIZE)
#define RH_FLASH_WORD_BYTES (FLASH_NB_32BITWORD_IN_FLASHWORD * 4u)

extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;

static uint32_t slot_address(unsigned slot)
{
    return RH_KEYSTORE_BASE + (uint32_t)slot * FLASH_SECTOR_SIZE;
}

/* The image's last byte in flash is below the keystore. */
static int image_clear_of_keystore(void)
{
    uintptr_t end = (uintptr_t)&_sidata + ((uintptr_t)&_edata - (uintptr_t)&_sdata);
    return end <= RH_KEYSTORE_BASE;
}

static void invalidate(uint32_t address, uint32_t len)
{
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    SCB_InvalidateDCache_by_Addr((void *)address, (int32_t)len);
#else
    (void)address;
    (void)len;
#endif
}

static int stm32_read(void *ctx, unsigned slot, uint8_t *buf, size_t len)
{
    (void)ctx;
    if (slot >= RH_KEYSTORE_SLOTS || len > FLASH_SECTOR_SIZE) {
        return -1;
    }
    invalidate(slot_address(slot), (uint32_t)len);
    memcpy(buf, (const void *)(uintptr_t)slot_address(slot), len);
    return 0;
}

static int stm32_erase(void *ctx, unsigned slot)
{
    (void)ctx;
    if (slot >= RH_KEYSTORE_SLOTS || !image_clear_of_keystore()) {
        return -1;
    }
    FLASH_EraseInitTypeDef erase;
    memset(&erase, 0, sizeof(erase));
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Banks = FLASH_BANK_2;
    erase.Sector = RH_KEYSTORE_FIRST_SECTOR + slot;
    erase.NbSectors = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    uint32_t failed = 0;

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return -1;
    }
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &failed);
    (void)HAL_FLASH_Lock();
    invalidate(slot_address(slot), FLASH_SECTOR_SIZE);
    return status == HAL_OK ? 0 : -1;
}

static int stm32_write(void *ctx, unsigned slot, const uint8_t *buf, size_t len)
{
    (void)ctx;
    if (slot >= RH_KEYSTORE_SLOTS || len > RH_KEYSTORE_RECORD_SIZE || !image_clear_of_keystore()) {
        return -1;
    }
    /* Whole flash words, the tail padded with the erased value. */
    static uint32_t words[(RH_KEYSTORE_RECORD_SIZE + RH_FLASH_WORD_BYTES - 1u) / RH_FLASH_WORD_BYTES *
                          FLASH_NB_32BITWORD_IN_FLASHWORD];
    memset(words, 0xFF, sizeof(words));
    memcpy(words, buf, len);

    HAL_StatusTypeDef status = HAL_FLASH_Unlock();
    for (uint32_t offset = 0; status == HAL_OK && offset < sizeof(words); offset += RH_FLASH_WORD_BYTES) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, slot_address(slot) + offset,
                                   (uint32_t)(uintptr_t)((const uint8_t *)words + offset));
    }
    (void)HAL_FLASH_Lock();
    memset(words, 0, sizeof(words));
    invalidate(slot_address(slot), sizeof(words));
    return status == HAL_OK ? 0 : -1;
}

static const rh_keystore_backend_t stm32_backend = { 0, stm32_read, stm32_erase, stm32_write };

int rh_keystore_load(rh_key_t *out)
{
    return rh_keystore_core_load(&stm32_backend, out);
}

int rh_keystore_store(const rh_key_t *key)
{
    return rh_keystore_core_store(&stm32_backend, key);
}

#endif /* RH_KEYSTORE_STM32_FLASH on a dual-bank STM32H7 */
