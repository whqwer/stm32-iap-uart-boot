/**
 * @file iap_image.c
 * @brief Dual Image (A/B) Management Implementation (Optimized for size)
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
    return (config->magic == CONFIG_MAGIC) ? 0 : -1;
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
    
    for (int i = 0; i < 2; i++) {
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
 * @brief Initialize config sector with default values
 * @return 0=success, -1=failed
 */
int8_t Config_Init(void)
{
    ImageConfig_t default_config = IMAGE_CONFIG_DEFAULT;
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
    if (size == 0 || size > IMAGE_A_SIZE) return 0;
    return crc32_c((uint8_t*)image_base, size);
}

/**
 * @brief Verify image region validity (CRC and stack pointer check)
 * @param image_index 0=A, 1=B
 * @param config Image management structure
 * @return 0=valid, -1=invalid
 */
int8_t Verify_Image(uint8_t image_index, const ImageConfig_t *config)
{
    uint32_t image_base = (image_index == 0) ? IMAGE_A_BASE : IMAGE_B_BASE;
    uint32_t expected_crc = (image_index == 0) ? config->crc_A : config->crc_B;
    uint32_t expected_size = (image_index == 0) ? config->size_A : config->size_B;
    
    if (expected_size == 0) return -1;
    
    /* Check stack pointer validity */
    uint32_t sp = *(__IO uint32_t*)image_base;
    if (sp < 0x20000000 || sp > 0x20008000) return -1;
    
    /* Compare CRC */
    return (Calculate_Image_CRC(image_base, expected_size) == expected_crc) ? 0 : -1;
}

/*============================================================================
 * Boot Image Selection
 *============================================================================*/

/**
 * @brief Select valid image region as boot entry on startup
 * @param config Image management structure (may be updated)
 * @return Image entry address (A or B), 0=no valid image
 */
uint32_t Select_Boot_Image(ImageConfig_t *config)
{
    /* Case 1: Update interrupted */
    if (config->updating) {
        uint8_t failed_image = 1 - config->active_image;
        if (failed_image == 0) { config->size_A = 0; config->crc_A = 0; }
        else { config->size_B = 0; config->crc_B = 0; }
        config->updating = 0;
        config->boot_count = 0;
        Config_Write(config);
    }
    
    /* Case 2: Too many failures */
    if (config->boot_count >= MAX_BOOT_ATTEMPTS) {
        config->active_image = 1 - config->active_image;
        config->boot_count = 0;
        Config_Write(config);
    }
    
    /* Case 3: Try active then backup */
    for (uint8_t attempt = 0; attempt < 2; attempt++) {
        uint8_t try_image = (attempt == 0) ? config->active_image : (1 - config->active_image);
        
        if (Verify_Image(try_image, config) == 0) {
            uint32_t boot_addr = (try_image == 0) ? IMAGE_A_BASE : IMAGE_B_BASE;
            if (try_image != config->active_image) {
                config->active_image = try_image;
            }
            config->boot_count++;
            Config_Write(config);
            return boot_addr;
        }
    }
    
    return 0;  /* No valid image */
}

/*============================================================================
 * Update Operations
 *============================================================================*/

/**
 * @brief Select update target region (inactive image)
 * @param config Image management structure
 * @return 0=A, 1=B
 */
uint8_t Select_Update_Target(const ImageConfig_t *config)
{
    return 1 - config->active_image;
}

/**
 * @brief Get Flash write address for update target region
 * @param config Image management structure
 * @return Target region start address
 */
uint32_t Get_Update_Address(const ImageConfig_t *config)
{
    return (Select_Update_Target(config) == 0) ? IMAGE_A_BASE : IMAGE_B_BASE;
}

/**
 * @brief Mark update start, set updating flag
 * @param config Image management structure
 * @param target_image Target region index
 */
void Update_Start(ImageConfig_t *config, uint8_t target_image)
{
    (void)target_image;
    config->updating = 1;
    Config_Write(config);
}

/**
 * @brief Update complete, save new image info and switch active region
 * @param config Image management structure
 * @param target_image Target region index
 * @param size New image length
 * @param crc New image CRC32
 */
void Update_Complete(ImageConfig_t *config, uint8_t target_image, 
                     uint32_t size, uint32_t crc)
{
    if (target_image == 0) { config->size_A = size; config->crc_A = crc; }
    else { config->size_B = size; config->crc_B = crc; }
    
    config->active_image = target_image;
    config->updating = 0;
    config->boot_count = 0;
    Config_Write(config);
}

/**
 * @brief Update failed, clear updating flag, keep original active region
 * @param config Image management structure
 */
void Update_Failed(ImageConfig_t *config)
{
    config->updating = 0;
    Config_Write(config);
}

/**
 * @brief After successful app boot, reset boot count
 * @param config Image management structure
 */
void Confirm_Boot_Success(ImageConfig_t *config)
{
    if (config->boot_count > 0) {
        config->boot_count = 0;
        Config_Write(config);
    }
}

/*============================================================================
 * Flash Erase
 *============================================================================*/

/**
 * @brief Erase specified image region Flash space (auto handle Bank1/Bank2)
 * @param target_image 0=A, 1=B
 * @return 1=success, 0=failed
 */
uint8_t Erase_Image(uint8_t target_image)
{
    HAL_StatusTypeDef status;
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;
    
    uint32_t image_base = (target_image == 0) ? IMAGE_A_BASE : IMAGE_B_BASE;
    uint32_t start_sector = (image_base - STM32_FLASH_BASE) / PAGE_SIZE;
    uint32_t num_sectors = IMAGE_A_SIZE / PAGE_SIZE;
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
