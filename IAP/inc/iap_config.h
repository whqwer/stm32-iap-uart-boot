#ifndef __IAP_CONFIG_H__
#define __IAP_CONFIG_H__
/* Define if use bkp save flag  -------------------------------*/
#define USE_BKP_SAVE_FLAG     1

/* Define the APP start address -------------------------------*/
#define ApplicationAddress    0x800C000

/* Output printer switch --------------------------------------*/
#define ENABLE_PUTSTR         1

/* IAP command------------------------------------------------ */
#if (USE_BKP_SAVE_FLAG == 1)
  #define IAP_FLAG_ADDR   (uint32_t)(ApplicationAddress - 1024 * 8)  // Address for App to write bootloader update flag (sector size is 8KB)
#else
  #define IAP_FLAG_ADDR   (uint32_t)(ApplicationAddress - 1024 * 8)  // Address for App to write bootloader update flag (sector size is 8KB)
#endif

 #define PAGE_SIZE                         (0x2000)    /* 8 Kbyte */
 #define APP_FLASH_SIZE                    (0x14000)  /* 80 KBytes */

/* STM32H5 Flash Definitions ---------------------------------*/
#define STM32_FLASH_BASE                   (0x08000000)    /* Flash base address */
#define STM_SECTOR_SIZE                    PAGE_SIZE       /* Sector size = 8KB */
#define STMFLASH_BUF_SIZE                  (PAGE_SIZE / 2) /* Buffer size in half-words */

/* Compute the FLASH upload image size --------------------------*/  
#define FLASH_IMAGE_SIZE                   (uint32_t) (APP_FLASH_SIZE - (ApplicationAddress - 0x08000000))

///* The maximum length of the command string -------------------*/
//#define CMD_STRING_SIZE       128

#endif
