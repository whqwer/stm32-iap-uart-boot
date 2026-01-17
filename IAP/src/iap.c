#include "iap_config.h"
#include "iap.h"
#include "stmflash.h"
#include "ymodem.h"
#include "protocol.h"

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
// 基于协议的固件更新（新版本）
int8_t IAP_Update(void)
{
    uint8_t rx_buffer[512];  // UART接收缓冲区（一次接收最多512字节）
    uint16_t rx_len = 0;
    uint32_t start_time;
    uint32_t last_progress = 0;
    HAL_StatusTypeDef status;
    
    // 1. 初始化协议层
    Protocol_IAP_Init();
    
    // 2. 擦除Flash
    SerialPutString("\r\n=== IAP Update Mode ===\r\n");
    SerialPutString("Erasing flash...\r\n");
    if (!EraseSomePages(FLASH_IMAGE_SIZE, 1))
    {
        SerialPutString("Erase failed!\r\n");
        return -1;
    }
    
    SerialPutString("Ready to receive firmware data...\r\n");
    SerialPutString("Waiting for data (timeout: 30s)...\r\n");
    
    start_time = HAL_GetTick();
    
    // 3. 主循环：使用 ReceiveToIdle 一次接收多个字节
    while (1)
    {
        // 检查总超时 (30秒)
        if ((HAL_GetTick() - start_time) > 30000)
        {
            SerialPutString("\r\n=== Update Timeout! ===\r\n");
            return -2;
        }
        
        // 使用 ReceiveToIdle 接收数据，遇到总线空闲或缓冲区满就返回
        // 超时设置为5秒，如果5秒内没有数据开始接收则超时
        status = HAL_UARTEx_ReceiveToIdle(&huart1, rx_buffer, sizeof(rx_buffer), &rx_len, 5000);
        
        if (status == HAL_OK && rx_len > 0)
        {
            // 收到数据，调用协议解析
            Protocol_Receive(rx_buffer, rx_len);
            
            // 重置超时计时（有数据就继续等待）
            start_time = HAL_GetTick();
            
            // 打印进度
            uint32_t current_progress = Protocol_IAP_GetProgress();
            if (current_progress != last_progress)
            {
                uint8_t progress_str[12];
                Int2Str(progress_str, current_progress);
                SerialPutString("\rReceived: ");
                SerialPutString(progress_str);
                SerialPutString(" bytes");
                last_progress = current_progress;
            }
            
            // 如果接收到的数据量小于缓冲区大小，说明是最后一包
            if (rx_len < sizeof(rx_buffer))
            {
                // 等待一小段时间确认没有更多数据
                HAL_Delay(500);
                
                // 再尝试接收一次，如果还有数据则继续
                uint16_t extra_len = 0;
                status = HAL_UARTEx_ReceiveToIdle(&huart1, rx_buffer, sizeof(rx_buffer), &extra_len, 500);
                
                if (status == HAL_OK && extra_len > 0)
                {
                    // 还有数据，继续处理
                    Protocol_Receive(rx_buffer, extra_len);
                    continue;
                }
                
                // 确认传输完成
                uint32_t total_received = Protocol_IAP_GetProgress();
                if (total_received > 0)
                {
                    SerialPutString("\r\n=== Update Successful! ===\r\n");
                    uint8_t size_str[12];
                    Int2Str(size_str, total_received);
                    SerialPutString("Total received: ");
                    SerialPutString(size_str);
                    SerialPutString(" bytes\r\n");
                    return 0;
                }
            }
        }
        else if (status == HAL_TIMEOUT)
        {
            // 如果已经接收过数据，超时认为传输完成
            uint32_t total_received = Protocol_IAP_GetProgress();
            if (total_received > 0)
            {
                SerialPutString("\r\n=== Update Successful! ===\r\n");
                uint8_t size_str[12];
                Int2Str(size_str, total_received);
                SerialPutString("Total received: ");
                SerialPutString(size_str);
                SerialPutString(" bytes\r\n");
                return 0;
            }
            // 如果还没有接收任何数据，继续等待
        }
        else
        {
            // 其他错误
            SerialPutString("\r\nUART receive error!\r\n");
            return -1;
        }
        
        HAL_Delay(10);
    }
}

/************************************************************************/
// 基于Ymodem的固件更新（备份版本）
int8_t IAP_Update_Ymodem(void)
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
	

