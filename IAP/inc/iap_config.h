#ifndef __IAP_CONFIG_H__
#define __IAP_CONFIG_H__

/*============================================================================
 * STM32H503 Flash Memory Layout (128KB Total)
 *============================================================================
 * Bootloader:  0x08000000 - 0x08008000 (32KB, sectors 0-3)
 * App A:       0x08008000 - 0x08014000 (48KB, sectors 4-9)
 * App B:       0x08014000 - 0x08020000 (48KB, sectors 10-15)
 * 
 * Config Data: Stored at 0x08006000 (last 8KB sector of Bootloader)
 *============================================================================*/
#include <stdint.h>
/* Output printer switch --------------------------------------*/
#define ENABLE_PUTSTR         1

/* Flash Memory Layout ----------------------------------------*/
#define STM32_FLASH_BASE      0x08000000    /* Flash base address */
#define PAGE_SIZE             0x2000        /* 8 Kbyte per sector */
#define STM_SECTOR_SIZE       PAGE_SIZE     /* Sector size = 8KB */
#define STMFLASH_BUF_SIZE     (PAGE_SIZE/2) /* Buffer size in half-words */

/* Bootloader Region ------------------------------------------*/
#define BOOTLOADER_BASE       0x08000000
#define BOOTLOADER_SIZE       (32 * 1024)   /* 32KB */

/* Config Sector (last sector of Bootloader) ------------------*/
#define CONFIG_BASE           0x08006000    /* Sector 3 */
#define CONFIG_SIZE           PAGE_SIZE     /* 8KB */

/* App A Region -----------------------------------------------*/
#define IMAGE_A_BASE          0x08008000    /* Sector 4 */
#define IMAGE_A_SIZE          (48 * 1024)   /* 48KB (6 sectors) */

/* App B Region -----------------------------------------------*/
#define IMAGE_B_BASE          0x08014000    /* Sector 10 */
#define IMAGE_B_SIZE          (48 * 1024)   /* 48KB (6 sectors) */

/* Compatibility: Default to Image A as ApplicationAddress ----*/
#define ApplicationAddress    IMAGE_A_BASE
#define FLASH_IMAGE_SIZE      IMAGE_A_SIZE

/*============================================================================
 * Image Configuration Structure (Simplified)
 *============================================================================
 * Only 3 essential states:
 * - active_image: Which image to run (0=A, 1=B)
 * - updating: Is update in progress? (prevents half-written images)
 * - boot_count: Detect boot failures for auto-rollback
 *============================================================================*/

#define CONFIG_MAGIC          0x41535444    /* "ASTD" in ASCII */
#define MAX_BOOT_ATTEMPTS     3             /* Max boot failures before rollback */

/* Simplified Image Config Structure (32 bytes) */
typedef struct {
    uint32_t magic;           /* Must be CONFIG_MAGIC (0x41535444) */
    uint8_t  active_image;    /* 0=App A, 1=App B */
    uint8_t  updating;        /* 0=idle, 1=updating (protect against power loss) */
    uint8_t  boot_count;      /* Boot failure counter (reset on app confirm) */
    uint8_t  reserved;        /* Alignment padding */
    uint32_t crc_A;           /* CRC32 of App A */
    uint32_t crc_B;           /* CRC32 of App B */
    uint32_t size_A;          /* Size of App A in bytes (0=empty) */
    uint32_t size_B;          /* Size of App B in bytes (0=empty) */
} ImageConfig_t;

/* Default config (all images invalid) */
#define IMAGE_CONFIG_DEFAULT { \
    .magic = CONFIG_MAGIC,     \
    .active_image = 0,         \
    .updating = 0,             \
    .boot_count = 0,           \
    .reserved = 0,             \
    .crc_A = 0,                \
    .crc_B = 0,                \
    .size_A = 0,               \
    .size_B = 0                \
}

#endif /* __IAP_CONFIG_H__ */
