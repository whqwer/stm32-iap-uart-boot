/**
  ******************************************************************************
  * @file    IAP/src/common.c 
  * @author  MCD Application Team
  * @version V3.3.0
  * @date    10/15/2010
  * @brief   This file provides all the common functions.
  ******************************************************************************
  * @copy
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2010 STMicroelectronics</center></h2>
  */ 

/** @addtogroup IAP
  * @{
  */

/* Includes ------------------------------------------------------------------*/
#include "common.h"
#include <string.h>
#include <stdlib.h>
extern UART_HandleTypeDef huart1;
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
  * @brief  Configures COM port.
  * @param  COM: Specifies the COM port to be configured.
  *   This parameter can be one of following parameters:    
  *     @arg COM1
  *     @arg COM2  
  * @param  USART_InitStruct: pointer to a USART_InitTypeDef structure that
  *   contains the configuration information for the specified USART peripheral.
  * @retval None
  */
void STM_EVAL_COMInit(UART_InitTypeDef* UART_InitStruct)
{
  /* This function is not used in HAL implementation, USART is initialized in main.c */
  /* USART initialization is handled by MX_USART1_UART_Init() in main.c */
}


/**
  * @brief  Convert an Integer to a string
  * @param  str: The string buffer to store the result
  * @param  intnum: The integer to be converted
  * @retval None
  * @details Converts a positive integer to ASCII string representation.
  *          Leading zeros are automatically trimmed. Uses division by powers of 10
  *          starting from 1 billion, extracting each digit and converting to ASCII.
  */
void Int2Str(uint8_t* str, int32_t intnum)
{
	uint32_t i, Div = 1000000000, j = 0, Status = 0;

	for (i = 0; i < 10; i++)  // Process up to 10 digits (max for 32-bit int)
	{
		str[j++] = (intnum / Div) + 48;  // Convert digit to ASCII ('0' = 48)

		intnum = intnum % Div;  // Remove processed digit
		Div /= 10;  // Move to next decimal place
		// Trim leading zeros: reset index if current digit is '0' and no non-zero digit has been encountered
		if ((str[j-1] == '0') & (Status == 0))
		{
			j = 0;  // Reset write position to skip leading zero
		}
		else
		{
			Status++;  // Mark that we've encountered a non-zero digit
		}
	}
}

/**
  * @brief  Send a string to UART serial port
  * @param  s: Pointer to null-terminated string to transmit
  * @retval None
  * @details Transmits string via UART1 with 1000ms timeout.
  *          Can be disabled via ENABLE_PUTSTR macro for production builds.
  */
void Serial_PutString(uint8_t *s)
{
#if (ENABLE_PUTSTR == 1)  // Conditional compilation for debug output
	HAL_UART_Transmit(&huart1, s, strlen((const char*)s),1000);
#endif
}

/**
  * @}
  */

/**
  * @brief  Calculate the number of pages
  * @param  Size: The image size
  * @retval The number of pages
  */
//uint32_t FLASH_PagesMask(__IO uint32_t Size)
//{
//	uint32_t pagenumber = 0x0;
//	uint32_t size = Size;
//
//	if ((size % PAGE_SIZE) != 0)
//	{
//		pagenumber = (size / PAGE_SIZE) + 1;
//	}
//	else
//	{
//		pagenumber = size / PAGE_SIZE;
//	}
//	return pagenumber;
//}

/**
  * @brief  Erase Flash sectors for application update
  * @param  size: Size in bytes of the area to erase
  * @param  outPutCont: If 1, output erase progress via serial port
  * @retval 1 on success, 0 on failure
  * @details Erases Flash sectors starting from ApplicationAddress.
  *          Handles dual-bank Flash architecture of STM32H5:
  *          - Bank 1: Sectors 0-7 (each 8KB)
  *          - Bank 2: Sectors 0-7 (each 8KB)
  *          Automatically splits erase operation across banks when needed.
  */
//uint8_t EraseSomePages(__IO uint32_t size, uint8_t outPutCont)
//{
//	    (void)outPutCont;  /* Unused */
//	    HAL_StatusTypeDef status = HAL_OK;
//	    FLASH_EraseInitTypeDef EraseInitStruct;
//	    uint32_t SectorError = 0;
//
//	    // Calculate global starting sector and total sectors needed
//	    uint32_t global_start_sector = (ApplicationAddress - FLASH_BASE) / PAGE_SIZE;  // e.g., = 6 for 0x0800C000
//	    uint32_t total_sectors = FLASH_PagesMask(size);  // e.g., = 4 for 32KB
//
//	    uint32_t sectors_per_bank = 8;  // STM32H503: 8 sectors per bank
//
//	    HAL_FLASH_Unlock();
//	    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
//
//	    // Erase Bank 1 (if starting sector is in Bank 1)
//	    if (global_start_sector < sectors_per_bank) {
//	        // Calculate how many sectors can be erased in Bank 1
//	        // If all sectors fit in Bank 1, erase them all; otherwise, erase to end of Bank 1
//	        uint32_t bank1_sectors = (global_start_sector + total_sectors <= sectors_per_bank)
//	                                 ? total_sectors
//	                                 : (sectors_per_bank - global_start_sector);
//
//	        EraseInitStruct.Banks = FLASH_BANK_1;
//	        EraseInitStruct.Sector = global_start_sector;  // e.g., sector 6 in Bank 1
//	        EraseInitStruct.NbSectors = bank1_sectors;
//	        status = HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
//
//	        total_sectors -= bank1_sectors;  // Subtract erased sectors from total
//	    }
//
//	    // Erase Bank 2 (if there are remaining sectors to erase)
//	    if (total_sectors > 0 && status == HAL_OK) {
//	        EraseInitStruct.Banks = FLASH_BANK_2;
//	        EraseInitStruct.Sector = 0;  // Bank 2 starts from sector 0
//	        EraseInitStruct.NbSectors = total_sectors;
//	        status = HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
//	    }
//
//	HAL_FLASH_Lock();
//
//	if(status != HAL_OK)
//	{
//		return 0;  // Erase failed
//	}
//	return 1;  // Erase successful
//}


/*******************(C)COPYRIGHT 2010 STMicroelectronics *****END OF FILE******/
