#include "iap_config.h"
#include "iap.h"
#include "stmflash.h"
#include "ymodem.h"
#include "protocol.h"
#include <stdlib.h>

pFunction Jump_To_Application;
uint32_t JumpAddress;
uint32_t BlockNbr = 0, UserMemoryMask = 0;
__IO uint32_t FlashProtection = 0;
// uint8_t tab_1024[1024] = {0};


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
extern UART_HandleTypeDef huart1;
int8_t IAP_RunApp(void)
{
		// 读取应用程序的初始栈指针
	uint32_t sp = (*(__IO uint32_t*)ApplicationAddress);
	
	// 验证栈指针是否有效 (STM32H503 SRAM: 0x20000000-0x20008000, 32KB)
	// 栈指针应该在 SRAM 起始地址和结束地址之间（含结束地址）
	if (sp >= 0x20000000 && sp <= 0x20008000)
	{   
		SerialPutString("\r\n Run to app.\r\n");
		
		// 1. 关闭全局中断
		__disable_irq();
		
		// 2. 去初始化外设
		HAL_UART_MspDeInit(&huart1);
		HAL_DeInit();
		
		// 3. 关闭 SysTick
		SysTick->CTRL = 0;
		SysTick->LOAD = 0;
		SysTick->VAL = 0;
		
		// 4. 清除所有挂起的中断
		for (uint8_t i = 0; i < 8; i++)
		{
			NVIC->ICER[i] = 0xFFFFFFFF;
			NVIC->ICPR[i] = 0xFFFFFFFF;
		}
		
		// 5. 设置向量表偏移到应用程序地址
		SCB->VTOR = ApplicationAddress;
		
		// 6. STM32H5 缓存处理 - 清除并禁用缓存
		#if (__ICACHE_PRESENT == 1)
		SCB_InvalidateICache();
		SCB_DisableICache();
		#endif
		#if (__DCACHE_PRESENT == 1)
		SCB_CleanInvalidateDCache();
		SCB_DisableDCache();
		#endif
		
		// 7. 设置主栈指针
		__set_MSP(*(__IO uint32_t*) ApplicationAddress);
		
		// 8. 获取复位向量地址并跳转
		JumpAddress = *(__IO uint32_t*) (ApplicationAddress + 4);
		Jump_To_Application = (pFunction) JumpAddress;
		
		// 9. 跳转到应用程序
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
#define MAX_FRAME_SIZE    1024*4
// UART接收缓冲区（一次接收最多1024*4字节）
uint8_t rx_buffer[MAX_FRAME_SIZE];

// 基于协议的固件更新（新版本）
extern uint8_t UART1_in_update_mode;  // 声明外部变量

int8_t IAP_Update(void)
{
    uint16_t rx_len = 0;
    uint32_t start_time;
//    uint32_t last_progress = 0;
    HAL_StatusTypeDef status;
    
    // 设置标志：进入 Update 模式
    UART1_in_update_mode = 1;
    
    // 关键修复：禁用UART中断，防止中断干扰阻塞式接收
    HAL_NVIC_DisableIRQ(USART1_IRQn);
    
    // 1. 初始化协议层
    Protocol_IAP_Init();
    
    // 2. 擦除Flash
    SerialPutString("\r\n=== IAP Update Mode ===\r\n");
    SerialPutString("Erasing flash...\r\n");
    if (!EraseSomePages(FLASH_IMAGE_SIZE, 1))
    {
        SerialPutString("Erase failed!\r\n");
        UART1_in_update_mode = 0;
        HAL_NVIC_EnableIRQ(USART1_IRQn);  // 重新启用UART中断
//        free(rx_buffer);
        return -1;
    }
    
//    SerialPutString("Ready to receive firmware data...\r\n");
//    SerialPutString("Waiting for data (timeout: 30s)...\r\n");
//
//    // 调试：检查 UART 初始状态
//    SerialPutString("[DEBUG] UART RxState: ");
//    if (huart1.RxState == HAL_UART_STATE_READY) SerialPutString("READY\r\n");
//    else if (huart1.RxState == HAL_UART_STATE_BUSY_RX) SerialPutString("BUSY_RX\r\n");
//    else SerialPutString("OTHER\r\n");
    
//    SerialPutString("[DEBUG] UART ErrorCode: ");
//    uint8_t err_str[12];
//    Int2Str(err_str, huart1.ErrorCode);
//    SerialPutString(err_str);
//    SerialPutString("\r\n");
    
    start_time = HAL_GetTick();

    // 3. 主循环：使用 ReceiveToIdle 一次接收多个字节
    while (1)
    {
        // 检查总超时 (30秒)
        if ((HAL_GetTick() - start_time) > 30000)
        {
            SerialPutString("\r\n=== Update Timeout! ===\r\n");
            UART1_in_update_mode = 0;  // 退出 Update 模式
            HAL_NVIC_EnableIRQ(USART1_IRQn);  // 重新启用UART中断
//            free(rx_buffer);
            return -2;
        }
        memset(rx_buffer,0,MAX_FRAME_SIZE);
        rx_len=0;
        // 清除之前可能的错误标志
        __HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_OREF);
        __HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_FEF);
        __HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_NEF);
        
        // 使用 ReceiveToIdle 接收数据，遇到总线空闲或缓冲区满就返回
        // 超时设置为5秒，如果5秒内没有数据开始接收则超时
        status = HAL_UARTEx_ReceiveToIdle(&huart1, rx_buffer, sizeof(rx_buffer), &rx_len, 10000);
        //HAL_UARTEx_ReceiveToIdle_IT(huart, rx_buffer, MAX_FRAME_SIZE);

		if (status == HAL_OK && rx_len>0)
		{
			// 收到数据，调用协议解析
			Protocol_Receive(rx_buffer, rx_len);
		}
		else if (status == HAL_TIMEOUT)
		{
			SerialPutString("\r\n[DEBUG] HAL_TIMEOUT detected\r\n");
			// 如果已经接收过数据，超时认为传输完成
			uint32_t total_received = Protocol_IAP_GetProgress();
			if (total_received > 0)
			{
				SerialPutString("\r\n=== Update Successful! ===\r\n");
				uint8_t size_str[12];
				Int2Str(size_str, total_received);
				SerialPutString("Total received: ");
				SerialPutString(size_str);
				UART1_in_update_mode = 0;  // 退出 Update 模式
			HAL_NVIC_EnableIRQ(USART1_IRQn);  // 重新启用UART中断
				return 0;
			}
			// 如果还没有接收任何数据，继续等待
			SerialPutString("\r\n[DEBUG] No data received yet, continue waiting...\r\n");
		}
		else if (status == HAL_OK && rx_len == 0)
		{
			// 收到 IDLE 但没有数据（可能是残留的 IDLE 标志）
			SerialPutString("\r\n[DEBUG] Received IDLE with no data, continue...\r\n");
			// 继续循环，不退出
		}
		else
		{
			// 其他错误
			SerialPutString("\r\nUART receive error! Status: ");
			if (status == HAL_ERROR) SerialPutString("HAL_ERROR");
			else if (status == HAL_BUSY) SerialPutString("HAL_BUSY");
			else SerialPutString("UNKNOWN");
			
			// 检查 UART 错误标志
			uint32_t error = HAL_UART_GetError(&huart1);
			if (error & HAL_UART_ERROR_ORE) SerialPutString(" ORE(Overrun)");
			if (error & HAL_UART_ERROR_FE) SerialPutString(" FE(Frame)");
			if (error & HAL_UART_ERROR_NE) SerialPutString(" NE(Noise)");
			if (error & HAL_UART_ERROR_PE) SerialPutString(" PE(Parity)");
			SerialPutString("\r\n");
			
			UART1_in_update_mode = 0;  // 退出 Update 模式
			HAL_NVIC_EnableIRQ(USART1_IRQn);  // 重新启用UART中断
//			free(rx_buffer);
			return -1;
		}

		HAL_Delay(10);

    }
}

// /************************************************************************/
// // 基于Ymodem的固件更新（备份版本）
// int8_t IAP_Update_Ymodem(void)
// {
// 	uint8_t Number[10] = "";
// 	int32_t Size = 0;
// 	Size = Ymodem_Receive(&tab_1024[0]);
// 	if (Size > 0)
// 	{
// 		SerialPutString("\r\n Update Over!\r\n");
// 		SerialPutString(" Name: ");
// 		SerialPutString(file_name);
// 		Int2Str(Number, Size);
// 		SerialPutString("\r\n Size: ");
// 		SerialPutString(Number);
// 		SerialPutString(" Bytes.\r\n");
// 		return 0;
// 	}
// 	else if (Size == -1)
// 	{
// 		SerialPutString("\r\n Image Too Big!\r\n");
// 		return -1;
// 	}
// 	else if (Size == -2)
// 	{
// 		SerialPutString("\r\n Update failed!\r\n");
// 		return -2;
// 	}
// 	else if (Size == -3)
// 	{
// 		SerialPutString("\r\n Aborted by user.\r\n");
// 		return -3;
// 	}
// 	else
// 	{
// 		SerialPutString(" Receive Filed.\r\n");
// 		return -4;
// 	}
// }


/************************************************************************/
//int8_t IAP_Upload(void)
//{
//	uint32_t status = 0;
//	SerialPutString("\n\n\rSelect Receive File ... (press any key to abort)\n\r");
//	if (GetKey() == CRC16)
//	{
//		status = Ymodem_Transmit((uint8_t*)ApplicationAddress, (const uint8_t*)"UploadedFlashImage.bin", FLASH_IMAGE_SIZE);
//		if (status != 0)
//		{
//			SerialPutString("\n\rError Occured while Transmitting File\n\r");
//			return -1;
//		}
//		else
//		{
//			SerialPutString("\n\rFile Trasmitted Successfully \n\r");
//			return -2;
//		}
//	}
//	else
//	{
//		SerialPutString("\r\n\nAborted by user.\n\r");
//		return 0;
//	}
//}


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
	

