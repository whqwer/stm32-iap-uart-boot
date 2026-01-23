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
  * @param  str: The string
  * @param  intnum: The intger to be converted
  * @retval None
  */
void Int2Str(uint8_t* str, int32_t intnum)
{
	uint32_t i, Div = 1000000000, j = 0, Status = 0;

	for (i = 0; i < 10; i++)
	{
		str[j++] = (intnum / Div) + 48;

		intnum = intnum % Div;
		Div /= 10;
		if ((str[j-1] == '0') & (Status == 0))
		{
			j = 0;
		}
		else
		{
			Status++;
		}
	}
}

/**
  * @brief  Convert a string to an integer
  * @param  inputstr: The string to be converted
  * @param  intnum: The intger value
  * @retval 1: Correct
  *         0: Error
  */
uint32_t Str2Int(uint8_t *inputstr, int32_t *intnum)
{
	uint32_t i = 0, res = 0;
	uint32_t val = 0;

	if (inputstr[0] == '0' && (inputstr[1] == 'x' || inputstr[1] == 'X'))
	{
		if (inputstr[2] == '\0')
		{
			return 0;
		}
		for (i = 2; i < 11; i++)
		{
			if (inputstr[i] == '\0')
			{
				*intnum = val;
				/* return 1; */
				res = 1;
				break;
			}
			if (ISVALIDHEX(inputstr[i]))
			{
				val = (val << 4) + CONVERTHEX(inputstr[i]);
			}
			else
			{
				/* return 0, Invalid input */
				res = 0;
				break;
			}
		}
		/* over 8 digit hex --invalid */
		if (i >= 11)
		{
			res = 0;
		}
	}
	else /* max 10-digit decimal input */
	{
		for (i = 0;i < 11;i++)
		{
			if (inputstr[i] == '\0')
			{
				*intnum = val;
				/* return 1 */
				res = 1;
				break;
			}
			else if ((inputstr[i] == 'k' || inputstr[i] == 'K') && (i > 0))
			{
				val = val << 10;
				*intnum = val;
				res = 1;
				break;
			}
			else if ((inputstr[i] == 'm' || inputstr[i] == 'M') && (i > 0))
			{
				val = val << 20;
				*intnum = val;
				res = 1;
				break;
			}
			else if (ISVALIDDEC(inputstr[i]))
			{
				val = val * 10 + CONVERTDEC(inputstr[i]);
			}
			else
			{
				/* return 0, Invalid input */
				res = 0;
				break;
			}
		}
		/* Over 10 digit decimal --invalid */
		if (i >= 11)
		{
			res = 0;
		}
	}

	return res;
}

/**
  * @brief  Get an integer from the HyperTerminal
  * @param  num: The inetger
  * @retval 1: Correct
  *         0: Error
  */
uint32_t GetIntegerInput(int32_t * num)
{
	uint8_t inputstr[16];

	while (1)
	{
		GetInputString(inputstr);
		if (inputstr[0] == '\0') continue;
		if ((inputstr[0] == 'a' || inputstr[0] == 'A') && inputstr[1] == '\0')
		{
			SerialPutString(" User Cancelled.\r\n");
			return 0;
		}

		if (Str2Int(inputstr, num) == 0)
		{
			SerialPutString(" Error, Input.\r\n");
		}
		else
		{
			return 1;
		}
	}
}

/**
  * @brief  Test to see if a key has been pressed on the HyperTerminal
  * @param  key: The key pressed
  * @retval 1: Correct
  *         0: Error
  */
uint32_t SerialKeyPressed(uint8_t *key)
{
	// Non-blocking: poll RXNE flag and read RDR directly
	if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) != RESET)
	{
		*key = (uint8_t)(huart1.Instance->RDR & 0xFF);
		return 1;
	}
	return 0;
}

/**
  * @brief  Get a key from the HyperTerminal
  * @param  None
  * @retval The Key Pressed
  */
uint8_t GetKey(void)
{
	uint8_t key = 0; 
	/* Blocking poll until a byte is available */
	while (SerialKeyPressed(&key) == 0) {}
	return key;

}

/**
  * @brief  Print a character on the HyperTerminal
  * @param  c: The character to be printed
  * @retval None
  */
void SerialPutChar(uint8_t c)
{
	// EVAL_COM1->TDR = c;
	// while ((EVAL_COM1->ISR & USART_ISR_TXE) == RESET)
	// {
	// }
	HAL_UART_Transmit(&huart1, &c, 1, HAL_MAX_DELAY);
}

/**
  * @brief  Print a string on the HyperTerminal
  * @param  s: The string to be printed
  * @retval None
  */
void Serial_PutString(uint8_t *s)
{
#if (ENABLE_PUTSTR == 1)
	// while (*s != '\0')
	// {
	// 	SerialPutChar(*s);
	// 	s++;
	// }
	HAL_UART_Transmit(&huart1, s, strlen((const char*)s),1000);
#endif
}

/**
  * @brief  Get Input string from the HyperTerminal
  * @param  buffP: The input string
  * @retval None
  */
void GetInputString (uint8_t * buffP)
{
	uint32_t bytes_read = 0;
	uint8_t c = 0;
	/* Ensure UART is ready and clear stale errors before blocking receive */
	HAL_UART_AbortReceive(&huart1);
	__HAL_UART_CLEAR_OREFLAG(&huart1);
	__HAL_UART_CLEAR_NEFLAG(&huart1);
	__HAL_UART_CLEAR_FEFLAG(&huart1);
	(void)huart1.Instance->RDR; // flush data register
	for(;;)
	{
		HAL_StatusTypeDef rx = HAL_UART_Receive(&huart1, &c, 1, HAL_MAX_DELAY);
		if (rx != HAL_OK)
		{
			if (rx == HAL_BUSY)
			{
				HAL_UART_AbortReceive(&huart1);
			}
			__HAL_UART_CLEAR_OREFLAG(&huart1);
			__HAL_UART_CLEAR_NEFLAG(&huart1);
			__HAL_UART_CLEAR_FEFLAG(&huart1);
			(void)huart1.Instance->RDR; // flush
			continue; // retry
		}
		if (c == '\r' || c == '\n')
		{
			SerialPutString("\r\n");
			break; // accept CR or LF as terminator
		}
		if (c == '\b') /* Backspace */
		{
			if (bytes_read > 0)
			{
				bytes_read --;
				SerialPutString("\b \b");
			}
			continue;
		}
		if (bytes_read >= CMD_STRING_SIZE - 1)
		{
			SerialPutString(" Cmd size over.\r\n");
			bytes_read = 0;
			continue;
		}
		if (c >= 0x20 && c <= 0x7E)
		{
			buffP[bytes_read++] = c;
			SerialPutChar(c);
		}
	}
	buffP[bytes_read] = '\0';
}

/**
  * @}
  */

/**
  * @brief  Calculate the number of pages
  * @param  Size: The image size
  * @retval The number of pages
  */
uint32_t FLASH_PagesMask(__IO uint32_t Size)
{
	uint32_t pagenumber = 0x0;
	uint32_t size = Size;

	if ((size % PAGE_SIZE) != 0)
	{
		pagenumber = (size / PAGE_SIZE) + 1;
	}
	else
	{
		pagenumber = size / PAGE_SIZE;
	}
	return pagenumber;
}

uint8_t EraseSomePages(__IO uint32_t size, uint8_t outPutCont)
{
	   uint32_t EraseCounter = 0x0;
	    uint8_t erase_cont[3] = {0};
	    HAL_StatusTypeDef status = HAL_OK;
	    FLASH_EraseInitTypeDef EraseInitStruct;
	    uint32_t SectorError = 0;

	    // 计算全局起始扇区和总扇区数
	    uint32_t global_start_sector = (ApplicationAddress - FLASH_BASE) / PAGE_SIZE;  // = 6
	    uint32_t total_sectors = FLASH_PagesMask(size);  // 例如 = 4

	    uint32_t sectors_per_bank = 8;  // 每个 Bank 8 个扇区

	    HAL_FLASH_Unlock();
	    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;

	    // Bank 1 擦除（如果起始在 Bank 1）
	    if (global_start_sector < sectors_per_bank) {
	        // 计算 Bank 1 能擦除的扇区数
	        uint32_t bank1_sectors = (global_start_sector + total_sectors <= sectors_per_bank)
	                                 ? total_sectors
	                                 : (sectors_per_bank - global_start_sector);

	        EraseInitStruct.Banks = FLASH_BANK_1;
	        EraseInitStruct.Sector = global_start_sector;  // Bank 1 的扇区 6
	        EraseInitStruct.NbSectors = bank1_sectors;
	        status = HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);

	        total_sectors -= bank1_sectors;
	    }

	    // Bank 2 擦除（如果还有剩余扇区）
	    if (total_sectors > 0 && status == HAL_OK) {
	        EraseInitStruct.Banks = FLASH_BANK_2;
	        EraseInitStruct.Sector = 0;  // Bank 2 从扇区 0 开始
	        EraseInitStruct.NbSectors = total_sectors;
	        status = HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
	    }

	if(status == HAL_OK)
	{
		for (EraseCounter = 0; EraseCounter < total_sectors; EraseCounter++)
		{
			if(outPutCont == 1)
			{
				Int2Str(erase_cont, EraseCounter + 1);
				SerialPutString(erase_cont);
				SerialPutString("@");
			}
		}
	}

	HAL_FLASH_Lock();

	if(status != HAL_OK)
	{
		return 0;
	}
	return 1;
}


/*******************(C)COPYRIGHT 2010 STMicroelectronics *****END OF FILE******/
