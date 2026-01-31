#ifndef __IAP_CONFIG_H__
#define __IAP_CONFIG_H__

/*============================================================================
 * STM32H503 Flash Memory Layout (128KB Total)
 *============================================================================
 * Bootloader:  0x08000000 - 0x08006000 (24KB, sectors 0-2)
 * Config:      0x08006000 - 0x08008000 (8KB, sector 3)
 * Update:      0x08008000 - 0x08014000 (48KB, sectors 4-9)
 * RunApp:      0x08014000 - 0x08020000 (48KB, sectors 10-15)
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
 * Only 2 essential fields:
 * - page_count: Page counter (2 bytes)
 * - firmware_CRC: Firmware CRC32 value (4 bytes)
 *============================================================================*/

/* Simplified Image Config Structure (6 bytes) */
typedef struct {
    uint16_t page_count;       /* 页计数（固件占用页数） */
    uint32_t firmware_CRC;     /* 固件CRC32值 */
} ImageConfig_t;

/* Default config */
#define IMAGE_CONFIG_DEFAULT { \
    .page_count = 0,           \
    .firmware_CRC = 0           \
}

#endif /* __IAP_CONFIG_H__ */
