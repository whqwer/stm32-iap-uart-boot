/**
 * @file iap_image.h
 * @brief Dual Image (A/B) Management for IAP
 */

#ifndef __IAP_IMAGE_H__
#define __IAP_IMAGE_H__

#include "iap_config.h"
#include <stdint.h>

/* Config operations */
int8_t Config_Read(ImageConfig_t *config);
int8_t Config_Write(const ImageConfig_t *config);
int8_t Config_Init(void);

/* CRC & Verification */
uint32_t Calculate_Image_CRC(uint32_t image_base, uint32_t size);
int8_t Verify_Image(uint8_t image_index, const ImageConfig_t *config);

/* Boot selection */
uint32_t Select_Boot_Image(ImageConfig_t *config);

/* Update operations */
uint8_t Select_Update_Target(const ImageConfig_t *config);
uint32_t Get_Update_Address(const ImageConfig_t *config);
void Update_Start(ImageConfig_t *config, uint8_t target_image);
void Update_Complete(ImageConfig_t *config, uint8_t target_image, uint32_t size, uint32_t crc);
void Update_Failed(ImageConfig_t *config);
void Confirm_Boot_Success(ImageConfig_t *config);

/* Flash erase */
uint8_t Erase_Image(uint8_t target_image);

#endif /* __IAP_IMAGE_H__ */
