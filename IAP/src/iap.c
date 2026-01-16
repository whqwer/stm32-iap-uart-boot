#include "iap_config.h"
#include "iap.h"
#include "stmflash.h"
#include "ymodem.h"

pFunction Jump_To_Application;
uint32_t JumpAddress;
uint32_t BlockNbr = 0, UserMemoryMask = 0;
__IO uint32_t FlashProtection = 0;
uint8_t tab_1024[1024] = {0};


/************************************************************************/

static void FLASH_DisableWriteProtectionPages(void)
{
	SerialPutString("Write protection control is not supported in this implementation.\r\n");
}


/************************************************************************/
void IAP_WriteFlag(uint16_t flag)
{
#if (USE_BKP_SAVE_FLAG == 1)
	// BKP is not supported in this implementation
	STMFLASH_Write(IAP_FLAG_ADDR, &flag, 1);
#else 
	STMFLASH_Write(IAP_FLAG_ADDR, &flag, 1);
#endif 	
}

/************************************************************************/
uint16_t IAP_ReadFlag(void)
{
#if (USE_BKP_SAVE_FLAG == 1)
	// BKP is not supported in this implementation
	return STMFLASH_ReadHalfWord(IAP_FLAG_ADDR);  
#else
	return STMFLASH_ReadHalfWord(IAP_FLAG_ADDR);  
#endif 	
}


/************************************************************************/
void IAP_UART_Init(void)
{
    // USART initialization is handled by MX_USART1_UART_Init() in main.c
}

void IAP_Init(void)
{
    IAP_UART_Init();
#if (USE_BKP_SAVE_FLAG == 1)
	// BKP is not supported in this implementation
#endif
}
/************************************************************************/
int8_t IAP_RunApp(void)
{
	if (((*(__IO uint32_t*)ApplicationAddress) & 0x2FFE0000 ) == 0x20000000)
	{   
		SerialPutString("\r\n Run to app.\r\n");
		JumpAddress = *(__IO uint32_t*) (ApplicationAddress + 4);
		Jump_To_Application = (pFunction) JumpAddress;
		__set_MSP(*(__IO uint32_t*) ApplicationAddress);
		Jump_To_Application();
		return 0;
	}
	else
	{
		SerialPutString("\r\n Run to app error.\r\n");
		return -1;
	}
}


/************************************************************************/
extern uint8_t UART1_flag;
extern UART_HandleTypeDef huart1;
uint8_t cmdStr[CMD_STRING_SIZE] = {0};
void IAP_Main_Menu(void)
{

	BlockNbr = (FlashDestination - 0x08000000) >> 12;
	
	// UserMemoryMask calculation for STM32H503CBT6
	UserMemoryMask = 0;
	
	// Flash protection is not supported in this implementation
	FlashProtection = 0;
	// 打印菜单一次，避免循环反复刷屏
	SerialPutString("\r\n IAP Main Menu (V 0.2.0)\r\n");
	SerialPutString(" update\r\n");
	SerialPutString(" upload\r\n");
	SerialPutString(" erase\r\n");
	SerialPutString(" menu\r\n");
	SerialPutString(" runapp\r\n");
	if(FlashProtection != 0)//There is write protected
	{
		SerialPutString(" diswp\r\n");
	}
	SerialPutString(" cmd> ");
	while (1)
	{

//		GetInputString(cmdStr);
		HAL_UARTEx_ReceiveToIdle_IT(&huart1, cmdStr, CMD_STRING_SIZE);
		if(UART1_flag){
			UART1_flag=0;
			if(strcmp((char *)cmdStr, CMD_UPDATE_STR) == 0)
			{
				IAP_WriteFlag(UPDATE_FLAG_DATA);
				return;
			}
			else if(strcmp((char *)cmdStr, CMD_UPLOAD_STR) == 0)
			{
				IAP_WriteFlag(UPLOAD_FLAG_DATA);
				return;
			}
			else if(strcmp((char *)cmdStr, CMD_ERASE_STR) == 0)
			{
				IAP_WriteFlag(ERASE_FLAG_DATA);
				return;
			}
			else if(strcmp((char *)cmdStr, CMD_MENU_STR) == 0)
			{
				IAP_WriteFlag(INIT_FLAG_DATA);
			}
			else if(strcmp((char *)cmdStr, CMD_RUNAPP_STR) == 0)
			{
				IAP_WriteFlag(APPRUN_FLAG_DATA);
				return;
			}
			else if(strcmp((char *)cmdStr, CMD_DISWP_STR) == 0)
			{
				FLASH_DisableWriteProtectionPages();
			}
			else
			{
				SerialPutString(" Invalid CMD !\r\n");
			}
			memset(cmdStr,0,CMD_STRING_SIZE);
		}
	}
}


/************************************************************************/
int8_t IAP_Update(void)
{
	uint8_t Number[10] = "";
	int32_t Size = 0;
	Size = Ymodem_Receive(&tab_1024[0]);
	if (Size > 0)
	{
		SerialPutString("\r\n Update Over!\r\n");
		SerialPutString(" Name: ");
		SerialPutString(file_name);
		Int2Str(Number, Size);
		SerialPutString("\r\n Size: ");
		SerialPutString(Number);
		SerialPutString(" Bytes.\r\n");
		return 0;
	}
	else if (Size == -1)
	{
		SerialPutString("\r\n Image Too Big!\r\n");
		return -1;
	}
	else if (Size == -2)
	{
		SerialPutString("\r\n Update failed!\r\n");
		return -2;
	}
	else if (Size == -3)
	{
		SerialPutString("\r\n Aborted by user.\r\n");
		return -3;
	}
	else
	{
		SerialPutString(" Receive Filed.\r\n");
		return -4;
	}
}


/************************************************************************/
int8_t IAP_Upload(void)
{
	uint32_t status = 0; 
	SerialPutString("\n\n\rSelect Receive File ... (press any key to abort)\n\r");
	if (GetKey() == CRC16)
	{
		status = Ymodem_Transmit((uint8_t*)ApplicationAddress, (const uint8_t*)"UploadedFlashImage.bin", FLASH_IMAGE_SIZE);
		if (status != 0) 
		{
			SerialPutString("\n\rError Occured while Transmitting File\n\r");
			return -1;
		}
		else
		{
			SerialPutString("\n\rFile Trasmitted Successfully \n\r");
			return -2;
		}
	}
	else
	{
		SerialPutString("\r\n\nAborted by user.\n\r");  
		return 0;
	}
}


/************************************************************************/
int8_t IAP_Erase(void)
{
	uint8_t erase_cont[3] = {0};
	Int2Str(erase_cont, FLASH_IMAGE_SIZE / PAGE_SIZE);
	SerialPutString(" @");//?�????���bug
	SerialPutString(erase_cont);
	SerialPutString("@");
	if(EraseSomePages(FLASH_IMAGE_SIZE, 1))
		return 0;
	else
		return -1;
}
	

