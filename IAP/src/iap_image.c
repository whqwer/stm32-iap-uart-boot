/**
 * @file iap_image.c
 * @brief Upgrade/RunApp Management Implementation (Optimized for size)
 */

#include "iap_image.h"
#include "stmflash.h"
#include "common.h"
#include <string.h>

/* External CRC32 function from protocol.c */
extern uint32_t crc32_c(const uint8_t *data, uint32_t len);

/*============================================================================
 * Config Sector Operations
 *============================================================================*/

/**
 * @brief Read image management structure from Flash config sector
 * @param config Pointer to destination structure
 * @return 0=success, -1=invalid or uninitialized
 */
int8_t Config_Read(ImageConfig_t *config)
{
    if (config == NULL) return -1;
    memcpy(config, (void*)CONFIG_BASE, sizeof(ImageConfig_t));
    return 0;
}

/**
 * @brief Write image management structure to Flash config sector
 * @param config Pointer to source structure
 * @return 0=success, -1=erase or write failed
 */
int8_t Config_Write(const ImageConfig_t *config)
{
    if (config == NULL) return -1;
    
    HAL_StatusTypeDef status;
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;
    uint32_t config_sector = (CONFIG_BASE - STM32_FLASH_BASE) / PAGE_SIZE;
    
    HAL_FLASH_Unlock();
    
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.Banks = FLASH_BANK_1;
    EraseInitStruct.Sector = config_sector;
    EraseInitStruct.NbSectors = 1;
    
    status = HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
    if (status != HAL_OK) {
        HAL_FLASH_Lock();
        return -1;
    }
    
    uint64_t *src = (uint64_t*)config;
    uint32_t addr = CONFIG_BASE;
    
    for (int i = 0; i < 1; i++) {
        __attribute__((aligned(16))) uint64_t qw_data[2];
        qw_data[0] = src[i * 2];
        qw_data[1] = src[i * 2 + 1];
        
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, addr, (uint32_t)qw_data);
        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            return -1;
        }
        addr += 16;
    }
    
    HAL_FLASH_Lock();
    return 0;
}

/**
 * @brief Check if a valid firmware exists at given address (by checking SP)
 * @param image_base Flash address
 * @return Estimated firmware size if valid, 0 if invalid
 */
static uint32_t Detect_Firmware_Size(uint32_t image_base)
{
    /* Check stack pointer validity */
    uint32_t sp = *(__IO uint32_t*)image_base;
    if (sp < 0x20000000 || sp > 0x20008000) {
        return 0;  /* Invalid stack pointer, no valid firmware */
    }
    
    /* Check reset vector validity */
    uint32_t reset_vector = *(__IO uint32_t*)(image_base + 4);
    if (reset_vector < image_base || reset_vector > (image_base + RUNAPP_REGION_SIZE)) {
        return 0;  /* Invalid reset vector */
    }
    
    /* Firmware exists, estimate size by scanning for end of code */
    /* Simple approach: scan backwards from max size to find non-0xFF data */
    uint32_t size = RUNAPP_REGION_SIZE;
    uint8_t *end_ptr = (uint8_t*)(image_base + size - 1);
    
    while (size > 0 && *end_ptr == 0xFF) {
        end_ptr--;
        size--;
    }
    
    /* Align to 16 bytes (quadword) */
    size = (size + 15) & ~15;
    
    /* Minimum size check */
    if (size < 256) {
        size = 256;  /* At least 256 bytes for vector table */
    }
    
    return size;
}

/**
 * @brief Initialize config sector with default values
 * @details If valid firmware is detected at 运行区, auto-record CRC
 * @return 0=success, -1=failed
 */
int8_t Config_Init(void)
{
    ImageConfig_t default_config = IMAGE_CONFIG_DEFAULT;
    
    /* Auto-detect existing firmware at 运行区 */
    uint32_t size_b = Detect_Firmware_Size(RUNAPP_REGION_BASE);
    if (size_b > 0) {
        default_config.firmware_CRC = crc32_c((uint8_t*)RUNAPP_REGION_BASE, size_b);
        /* 计算页数 */
        default_config.page_count = (size_b + PAGE_SIZE - 1) / PAGE_SIZE;
    }
    
    return Config_Write(&default_config);
}

/*============================================================================
 * CRC & Verification
 *============================================================================*/

/**
 * @brief Calculate CRC32 for specified image region
 * @param image_base Image start address
 * @param size Image length in bytes
 * @return CRC32 value
 */
uint32_t Calculate_Image_CRC(uint32_t image_base, uint32_t size)
{
    if (size == 0 || size > UPDATE_REGION_SIZE) return 0;
    return crc32_c((uint8_t*)image_base, size);
}

/**
 * @brief Verify 运行区 validity (CRC and stack pointer check)
 * @param config Image management structure
 * @return 0=valid, -1=invalid
 */
int8_t Verify_Run_Image(const ImageConfig_t *config)
{
    uint32_t image_base = RUNAPP_REGION_BASE;
    uint32_t expected_crc = config->firmware_CRC;
    
    if (expected_crc == 0) return -1;
    
    /* Check stack pointer validity */
    uint32_t sp = *(__IO uint32_t*)image_base;
    if (sp < 0x20000000 || sp > 0x20008000) return -1;
    
    /* Calculate current CRC */
    uint32_t current_crc = Calculate_Image_CRC(image_base, Detect_Firmware_Size(image_base));
    
    /* Compare CRC */
    return (current_crc == expected_crc) ? 0 : -1;
}

/*============================================================================
 * Boot Image Selection
 *============================================================================*/

/**
 * @brief Select valid image region as boot entry on startup
 * @param config Image management structure (may be updated)
 * @return Image entry address (运行区), 0=no valid image
 */
uint32_t Select_Boot_Image(ImageConfig_t *config)
{
    /* 检查page_count是否为0 */
    if (config->page_count == 0) {
        /* 不需要升级，检查运行区是否有效 */
        if (Verify_Run_Image(config) == 0) {
            return RUNAPP_REGION_BASE;
        }
    }
    
    /* page_count不为0，需要升级 */
    return 0;  /* No valid image, enter update mode */
}

/*============================================================================
 * Update Operations
 *============================================================================*/

/**
 * @brief Mark update start, set page_count
 * @param config Image management structure
 */
void Update_Start(ImageConfig_t *config, uint8_t target_image)
{
    config->page_count = 1;  /* 设置为1表示需要升级 */
    Config_Write(config);
}

/**
 * @brief Update failed, clear page_count
 * @param config Image management structure
 */
void Update_Failed(ImageConfig_t *config)
{
    config->page_count = 0;  /* 清除升级标志 */
    Config_Write(config);
}

/**
 * @brief After successful app boot, reset page_count
 * @param config Image management structure
 */
void Confirm_Boot_Success(ImageConfig_t *config)
{
    config->page_count = 0;  /* 清除升级标志 */
    Config_Write(config);
}

/*============================================================================
 * Flash Erase
 *============================================================================*/

/**
 * @brief Erase specified image region Flash space (auto handle Bank1/Bank2)
 * @param target_image 0=升级区, 1=运行区
 * @return 1=success, 0=failed
 */
uint8_t Erase_Image(uint8_t target_image)
{
    HAL_StatusTypeDef status;
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;
    
    uint32_t image_base = (target_image == 0) ? UPDATE_REGION_BASE : RUNAPP_REGION_BASE;
    uint32_t image_size = (target_image == 0) ? UPDATE_REGION_SIZE : RUNAPP_REGION_SIZE;
    uint32_t start_sector = (image_base - STM32_FLASH_BASE) / PAGE_SIZE;
    uint32_t num_sectors = image_size / PAGE_SIZE;
    uint32_t sectors_per_bank = 8;
    
    HAL_FLASH_Unlock();
    
    if (start_sector < sectors_per_bank) {
        uint32_t bank1_sectors = (start_sector + num_sectors <= sectors_per_bank) 
                                 ? num_sectors : (sectors_per_bank - start_sector);
        
        EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
        EraseInitStruct.Banks = FLASH_BANK_1;
        EraseInitStruct.Sector = start_sector;
        EraseInitStruct.NbSectors = bank1_sectors;
        
        status = HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
        if (status != HAL_OK) { HAL_FLASH_Lock(); return 0; }
        
        num_sectors -= bank1_sectors;
        start_sector = 0;
    } else {
        start_sector -= sectors_per_bank;
    }
    
    if (num_sectors > 0) {
        EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
        EraseInitStruct.Banks = FLASH_BANK_2;
        EraseInitStruct.Sector = start_sector;
        EraseInitStruct.NbSectors = num_sectors;
        
        status = HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
        if (status != HAL_OK) { HAL_FLASH_Lock(); return 0; }
    }
    
    HAL_FLASH_Lock();
    return 1;
}
