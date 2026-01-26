#include "iap_config.h"
#include "iap.h"
#include "stmflash.h"
#include "protocol.h"
#include <stdlib.h>

pFunction Jump_To_Application;
uint32_t JumpAddress;


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

	// 打印菜单一次，避免循环反复刷屏
	SerialPutString("\r\n IAP Main Menu (V 0.2.0)\r\n");
	SerialPutString(" update\r\n");
	SerialPutString(" upload\r\n");
	SerialPutString(" erase\r\n");
	SerialPutString(" menu\r\n");
	SerialPutString(" runapp\r\n");
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
#define MAX_FRAME_SIZE    1024*10
// UART接收缓冲区（一次接收最多1024*10字节）
extern uint8_t rx_buffer[MAX_FRAME_SIZE];

// 基于协议的固件更新（新版本）
extern uint8_t UART1_in_update_mode;  // 声明外部变量
extern uint8_t UART1_Complete_flag;
extern uint16_t rx_len;
int8_t IAP_Update(void)
{
    uint32_t start_time;
    HAL_StatusTypeDef status;
    uint8_t consecutive_idle = 0;  // 连续空闲次数
    
    // 设置标志：进入 Update 模式
    UART1_in_update_mode = 1;
    
    // 1. 初始化协议层
    Protocol_IAP_Init();
    
    // 2. 擦除Flash
    SerialPutString("\r\n=== IAP Update Mode ===\r\n");
    SerialPutString("Erasing flash...\r\n");
    if (!EraseSomePages(FLASH_IMAGE_SIZE, 1))
    {
        SerialPutString("Erase failed!\r\n");
        UART1_in_update_mode = 0;
        return -1;
    }
    
    start_time = HAL_GetTick();
    
    // ✅ 关键改动1：在循环外启动第一次DMA接收
    status = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer, sizeof(rx_buffer));
    if (status != HAL_OK) {
        SerialPutString("DMA start failed!\r\n");
        UART1_in_update_mode = 0;
        return -1;
    }
    huart1.hdmarx->XferHalfCpltCallback = NULL;
    SerialPutString("DMA started, waiting for data...\r\n");

    // ✅ 关键改动2：主循环只等待和处理数据
    while (1)
    {
        // 检查总超时 (30秒)
        if ((HAL_GetTick() - start_time) > 30000)
        {
            SerialPutString("\r\n=== Update Timeout (30s)! ===\r\n");
            UART1_in_update_mode = 0;
            return -2;
        }
        
        // ✅ 关键改动3：只判断标志，不判断status
        if(UART1_Complete_flag)
        {
            UART1_Complete_flag = 0;
            
            // ✅ 关键改动4：根据rx_len判断数据状态
            if (rx_len > 1)
            {
                
                // 处理数据
                Protocol_Receive(rx_buffer, rx_len);
                memset(rx_buffer, 0, MAX_FRAME_SIZE);
                
                // 重置空闲计数（收到数据说明传输还在进行）
                consecutive_idle = 0;

                // ✅ 关键改动5：处理完后重新启动DMA
                status = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer, sizeof(rx_buffer));
                if (status == HAL_OK) {
                    huart1.hdmarx->XferHalfCpltCallback = NULL;
                }
            }
            else if(rx_len == 1 && rx_buffer[0] == 0xFF)
            {
                // ✅ 关键改动6：这才是真正的"传输结束"判断逻辑
                consecutive_idle++;

                SerialPutString("\r\n[IDLE] Bus idle detected (");
                uint8_t count_str[4];
                Int2Str(count_str, consecutive_idle);
                SerialPutString(count_str);
                SerialPutString("/2)\r\n");

                // 连续2次IDLE且无数据，认为传输完成
                if (consecutive_idle >= 1)
                {
                    uint32_t total_received = Protocol_IAP_GetProgress();

                    if (total_received > 0)
                    {
                        // ✅ 传输成功完成
                        SerialPutString("\r\n=== Update Successful! ===\r\n");
                        uint8_t size_str[12];
                        Int2Str(size_str, total_received);
                        SerialPutString("Total received: ");
                        SerialPutString(size_str);
                        SerialPutString(" bytes\r\n");
                        UART1_in_update_mode = 0;
                        return 0;
                    }
                    else
                    {
                        // 没有收到任何数据就结束了
                        SerialPutString("\r\n=== No data received ===\r\n");
                        UART1_in_update_mode = 0;
                        return -3;
                    }
                }
                else
                {
                    // 第一次空闲，可能是包间隔，继续等待
                    SerialPutString("Waiting for more data...\r\n");

                    // 重新启动DMA
                    status = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer, sizeof(rx_buffer));
                    if (status == HAL_OK) {
                        huart1.hdmarx->XferHalfCpltCallback = NULL;
                    }
                }
            }
        }
        
        // 适当延时，避免CPU占用过高
        HAL_Delay(1);
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
	

