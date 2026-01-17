#ifndef __IAP_CONFIG_H__
#define __IAP_CONFIG_H__
/* Define if use bkp save flag  -------------------------------*/
#define USE_BKP_SAVE_FLAG     1

/* Define the APP start address -------------------------------*/
#define ApplicationAddress    0x800C000

/* Output printer switch --------------------------------------*/
#define ENABLE_PUTSTR         1

/* Bootloader command -----------------------------------------*/
#define CMD_UPDATE_STR        "update"
#define CMD_UPLOAD_STR        "upload"
#define CMD_ERASE_STR		  "erase"
#define CMD_MENU_STR          "menu"
#define CMD_RUNAPP_STR        "runapp"
#define CMD_ERROR_STR         "error"
#define CMD_DISWP_STR         "diswp"//禁止写保护

/* IAP command------------------------------------------------ */
#if (USE_BKP_SAVE_FLAG == 1)
  #define IAP_FLAG_ADDR   (uint32_t)(ApplicationAddress - 1024 * 8)//App�����Bootloader��������Ϣ�ĵ�ַ(�ݶ���СΪ2K)
#else
  #define IAP_FLAG_ADDR   (uint32_t)(ApplicationAddress - 1024 * 8)//App�����Bootloader��������Ϣ�ĵ�ַ(�ݶ���СΪ2K)
#endif

//#define IAP_FLASH_FLAG_ADDR   0x8002800

#if (USE_BKP_SAVE_FLAG == 1)
  #define INIT_FLAG_DATA      0x0000  //默认标志的数据(空片子的情况)
#else
  #define INIT_FLAG_DATA      0xFFFF   //默认标志的数据(空片子的情况)
#endif
#define UPDATE_FLAG_DATA      0xEEEE   //下载标志的数据
#define UPLOAD_FLAG_DATA      0xDDDD   //上传标志的数据
#define ERASE_FLAG_DATA       0xCCCC   //擦除标志的数据
#define APPRUN_FLAG_DATA      0x5A5A   //APP不需要做任何处理，直接运行状态

/* Define the Flsah area size ---------------------------------*/  
//#if defined (STM32F10X_MD) || defined (STM32F10X_MD_VL)
// #define PAGE_SIZE                         (0x400)    /* 1 Kbyte */
// #define APP_FLASH_SIZE                    (0x10000)  /* 64 KBytes */
//#elif defined STM32F10X_CL
// #define PAGE_SIZE                         (0x800)    /* 2 Kbytes */
// #define APP_FLASH_SIZE                    (0x40000)  /* 256 KBytes */
//#elif defined STM32F10X_HD || defined (STM32F10X_HD_VL)
// #define PAGE_SIZE                         (0x800)    /* 2 Kbytes */
// #define APP_FLASH_SIZE                    (0x80000)  /* 512 KBytes */
//#elif defined STM32F10X_XL
// #define PAGE_SIZE                         (0x800)    /* 2 Kbytes */
// #define APP_FLASH_SIZE                    (0x100000) /* 1 MByte */
//#else
// #error "Please select first the STM32 device to be used (in stm32f10x.h)"
//#endif
 #define PAGE_SIZE                         (0x2000)    /* 8 Kbyte */
 #define APP_FLASH_SIZE                    (0x14000)  /* 80 KBytes */

/* STM32H5 Flash Definitions ---------------------------------*/
#define STM32_FLASH_BASE                   (0x08000000)    /* Flash base address */
#define STM_SECTOR_SIZE                    PAGE_SIZE       /* Sector size = 8KB */
#define STMFLASH_BUF_SIZE                  (PAGE_SIZE / 2) /* Buffer size in half-words */

/* Compute the FLASH upload image size --------------------------*/  
#define FLASH_IMAGE_SIZE                   (uint32_t) (APP_FLASH_SIZE - (ApplicationAddress - 0x08000000))

/* The maximum length of the command string -------------------*/
#define CMD_STRING_SIZE       128

#endif
