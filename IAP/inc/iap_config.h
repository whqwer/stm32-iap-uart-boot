#ifndef __IAP_CONFIG_H__
#define __IAP_CONFIG_H__

/*============================================================================
 * STM32H503 Flash Memory Layout (128KB Total)
 *============================================================================
 * Bootloader:  0x08000000 - 0x08007FFF (32KB, sectors 0-3)
 * App:         0x08008000 - 0x0801DFFF (88KB, sectors 4-14)
 * Config:      0x0801E000 - 0x0801FFFF (8KB,  sector 15, last)
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

/* Config Sector (Flag Area) ------------------*/
#define CONFIG_BASE           0x08008000    /* Sector 4 (last) */
#define CONFIG_SIZE           (8 * 1024)    /* 8KB */

/* Update Region -----------------------------------------------*/
#define UPDATE_REGION_BASE    0x0800A000    /* Sector 5, APP 固件写入区 */
#define UPDATE_REGION_SIZE    (88 * 1024)   /* 88KB (11 sectors, 4-14) */

/* RunApp Region -----------------------------------------------
 * 当前为单区设计：UPDATE 和 RUNAPP 指向同一地址（0x0800E000）。
 * APP 链接脚本 ORIGIN = 0x08008000，固件直接写入并从此地址运行。
 * 无需 Copy_Update_To_Runapp()。                                 */
#define RUNAPP_REGION_BASE    0x0800A000    /* 与 UPDATE 同区，Sector 4 */
#define RUNAPP_REGION_SIZE    (88 * 1024)   /* 88KB */

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

/* Simplified Image Config Structure (10 bytes) */
typedef struct {
    uint16_t page_count;       /* Page count (number of pages used by firmware) */
    uint32_t firmware_CRC;     /* Firmware CRC32 value */
    uint32_t version;          /* Firmware version number (preserved during updates) */
    uint8_t  need_upgrade;     /* Explicit upgrade flag: 1=upgrade needed, 0=run app  */
} ImageConfig_t;

/* Default config */
#define IMAGE_CONFIG_DEFAULT { \
    .page_count = 0,           \
    .firmware_CRC = 0,         \
    .version      = 0,         \
    .need_upgrade = 0          \
}

#endif /* __IAP_CONFIG_H__ */
