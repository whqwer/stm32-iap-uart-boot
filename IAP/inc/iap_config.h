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
#define BOOTLOADER_SIZE       (24 * 1024)   /* 24KB */

/* Config Sector (标志区) ------------------*/
#define CONFIG_BASE           0x08006000    /* Sector 3 */
#define CONFIG_SIZE           (8 * 1024)    /* 8KB */

/* 升级区 Region -----------------------------------------------*/
#define UPDATE_REGION_BASE    0x08008000    /* Sector 4 */
#define UPDATE_REGION_SIZE    (48 * 1024)   /* 48KB (6 sectors) */

/* 运行区 Region -----------------------------------------------*/
#define RUNAPP_REGION_BASE    0x08014000    /* Sector 10 */
#define RUNAPP_REGION_SIZE    (48 * 1024)   /* 48KB (6 sectors) */

/* Compatibility: Default to UPDATE as ApplicationAddress ----*/
#define ApplicationAddress    UPDATE_REGION_BASE
#define FLASH_IMAGE_SIZE      UPDATE_REGION_SIZE

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

/* Simplified Image Config Structure (16 bytes) */
typedef struct {
    uint32_t magic;           /* Must be CONFIG_MAGIC (0x41535444) */
    uint8_t  update_flag;      /* 0=正常运行, 1=需要升级 */
    uint8_t  reserved[3];      /* Alignment padding */
    uint32_t run_crc;          /* CRC32 of 运行区 */
    uint32_t run_size;         /* Size of 运行区 in bytes (0=empty) */
} ImageConfig_t;

/* Default config */
#define IMAGE_CONFIG_DEFAULT { \
    .magic = CONFIG_MAGIC,     \
    .update_flag = 0,          \
    .reserved = {0},           \
    .run_crc = 0,              \
    .run_size = 0              \
}

#endif /* __IAP_CONFIG_H__ */
