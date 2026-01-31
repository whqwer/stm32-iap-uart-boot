/**
 * @file iap.c
 * @brief In-Application Programming (IAP) Implementation
 * 
 * This module handles:
 * - Jumping from bootloader to user application
 * - Protocol-based firmware update over UART
 * - Flash memory management for firmware storage
 * - Upgrade/RunApp management for fail-safe updates
 * 
 * Memory Layout:
 * - Bootloader: 0x08000000 - 0x08006000 (24KB)
 * - Config:     0x08006000 - 0x08008000 (8KB, last sector of bootloader)
 * - Update:     0x08008000 - 0x08014000 (48KB)
 * - RunApp:     0x08014000 - 0x08020000 (48KB)
 */

#include "iap_config.h"
#include "iap.h"
#include "iap_image.h"
#include "stmflash.h"
#include "protocol.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


pFunction Jump_To_Application;
uint32_t JumpAddress;

/* Global config for upgrade/runapp management */
static ImageConfig_t g_config;

/* Current update target address (used by protocol.c via extern) */
__attribute__((used)) uint32_t g_update_target_addr = UPDATE_REGION_BASE;




/**
 * @brief Initialize UART for IAP communication
 * @note USART initialization is actually handled by MX_USART1_UART_Init() in main.c
 *       This function is kept for compatibility
 */
void IAP_UART_Init(void)
{
    // USART initialization is handled by MX_USART1_UART_Init() in main.c
}

/**
 * @brief Initialize IAP module
 * @details Initializes UART and loads config for upgrade/runapp management
 */
void IAP_Init(void)
{
    IAP_UART_Init();
    
    /* Load or initialize upgrade/runapp config */
    if (Config_Read(&g_config) != 0) {
        /* Config invalid, initialize with defaults (auto-detect existing firmware) */
//        HAL_UART_Transmit(&huart1, (uint8_t*)"Config invalid, init...\r\n", 25, 100);
        Config_Init();
        Config_Read(&g_config);
    }
    
}

/************************************************************************/
extern UART_HandleTypeDef huart1;

/**
 * @brief Jump from bootloader to user application
 * @details Performs upgrade/runapp selection and jumps to the valid application.
 *          
 * Jump Process:
 * 1. Select boot image based on config and CRC verification
 * 2. Validates application stack pointer in SRAM range
 * 3. Disables all interrupts and peripherals
 * 4. Resets SysTick timer
 * 5. Clears pending interrupts
 * 6. Relocates vector table to application address
 * 7. Invalidates and disables caches (STM32H5 specific)
 * 8. Sets stack pointer to application's initial SP
 * 9. Jumps to application's reset handler
 * 
 * @return 0 if successful (should never return), -1 if no valid image
 * @note This function should never return if application is valid
 */
int8_t IAP_RunApp(void)
{
    /* Select boot image using upgrade/runapp logic */
    uint32_t boot_address = Select_Boot_Image(&g_config);

    /* Send boot_address to host PC via UART1 as string */
    const char *addr_str = (boot_address == 0x08008000) ? "0x08008000\r\n" :
                           (boot_address == 0x08014000) ? "0x08014000\r\n" : "0x00000000\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t *)"program start at ", strlen("program start at "), 100);
    HAL_UART_Transmit(&huart1, (uint8_t*)addr_str, strlen(addr_str), 100);

    if (boot_address == 0) {
        return -1;
    }

    /* Read application's initial stack pointer */
    uint32_t sp = (*(__IO uint32_t*)boot_address);

    /* Validate stack pointer (STM32H503 SRAM: 0x20000000-0x20008000, 32KB) */
    if (sp >= 0x20000000 && sp <= 0x20008000)
    {
        HAL_Delay(10);

        /* 1. Disable global interrupts */
        __disable_irq();

        /* 2. De-initialize peripherals */
        HAL_UART_MspDeInit(&huart1);
        HAL_DeInit();

        /* 3. Disable SysTick */
        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL = 0;

        /* 4. Clear all pending interrupts */
        for (uint8_t i = 0; i < 8; i++)
        {
            NVIC->ICER[i] = 0xFFFFFFFF;
            NVIC->ICPR[i] = 0xFFFFFFFF;
        }

        /* 5. Set vector table offset to application address */
        SCB->VTOR = boot_address;
        __DSB();  // 数据同步屏障
        __ISB();  // 指令同步屏障

        /* 6. STM32H5 cache handling - clear and disable cache */
        #if (__ICACHE_PRESENT == 1)
        SCB_InvalidateICache();
        SCB_DisableICache();
        #endif
        #if (__DCACHE_PRESENT == 1)
        SCB_CleanInvalidateDCache();
        SCB_DisableDCache();
        #endif

        /* 7. Set main stack pointer */
        __set_MSP(*(__IO uint32_t*) boot_address);

        /* 8. Get reset vector address and jump */
        JumpAddress = *(__IO uint32_t*) (boot_address + 4);
        Jump_To_Application = (pFunction) JumpAddress;

        /* 9. Jump to application */
        Jump_To_Application();

        return 0;
    }
    else
    {
        return -1;
    }
}




/************************************************************************/
#define MAX_FRAME_SIZE    1024*10
/* UART receive buffer (receive up to 1024*10 bytes at once) */
extern uint8_t rx_buffer[MAX_FRAME_SIZE];

/* Protocol-based firmware update (new version) */
extern uint8_t UART1_in_update_mode;  /* Declare external variable */
extern uint8_t UART1_Complete_flag;
extern uint16_t rx_len;

/**
 * @brief 复制升级区代码到运行区
 * @param size 代码大小
 * @return 0=成功, -1=失败
 */
static int8_t Copy_Update_To_Runapp(uint32_t size)
{
    // 计算需要复制的半字数量
    uint16_t num_halfwords = size / 2;
    if (size % 2 != 0) {
        num_halfwords += 1; // 处理奇数大小
    }
    
    // 直接使用STMFLASH_Write函数复制数据
    // STMFLASH_Write会自动处理擦除和写入操作
    STMFLASH_Write(RUNAPP_REGION_BASE, (uint16_t*)UPDATE_REGION_BASE, num_halfwords);
    
    return 0;
}

int8_t IAP_Update(void)
{
    uint32_t start_time;
    HAL_StatusTypeDef status;
    uint8_t consecutive_idle = 0;
    
    /* 固定升级到升级区 */
    g_update_target_addr = UPDATE_REGION_BASE;
    
    /* Set flag: Enter Update mode */
    UART1_in_update_mode = 1;
    
    /* 1. 标记升级开始 */
    g_config.page_count = 1;
    Config_Write(&g_config);
    
    /* 2. Initialize protocol layer */
    Protocol_IAP_Init();
    
    /* 3. Send target image info to host PC */
    HAL_UART_Transmit(&huart1, (uint8_t *)"fireware update start at:", strlen("fireware update start at:"), 100);
    HAL_UART_Transmit(&huart1, (uint8_t*)"0x08008000\r\n", strlen("0x08008000\r\n"), 100);
    
    /* 4. 擦除升级区 */
    uint8_t target_image = 0; // 0=升级区
    if (!Erase_Image(target_image))
    {
        g_config.page_count = 0;
        Config_Write(&g_config);
        UART1_in_update_mode = 0;
        return -1;
    }
    
    start_time = HAL_GetTick();
    
    /* 5. Start first DMA reception */
    status = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer, sizeof(rx_buffer));
    if (status != HAL_OK) {
        g_config.page_count = 0;
        Config_Write(&g_config);
        UART1_in_update_mode = 0;
        return -1;
    }
    huart1.hdmarx->XferHalfCpltCallback = NULL;

    /* 6. Main loop: wait and process data */
    while (1)
    {
        /* Check total timeout (20 seconds) */
        if ((HAL_GetTick() - start_time) > 20000)
        {
            g_config.page_count = 0;
            Config_Write(&g_config);
            UART1_in_update_mode = 0;
            return -2;
        }
        
        if(UART1_Complete_flag==1)
        {
            UART1_Complete_flag = 0;
            
            uint16_t remaining = (uint16_t)__HAL_DMA_GET_COUNTER(huart1.hdmarx);
            rx_len = sizeof(rx_buffer) - remaining;
            
            if (rx_len > 1)
            {
                /* Process data */
                Protocol_Receive(rx_buffer, rx_len);
                memset(rx_buffer, 0, MAX_FRAME_SIZE);
                
                /* Reset idle count */
                consecutive_idle = 0;

                /* Restart DMA after processing */
                HAL_UART_AbortReceive(&huart1);
                status = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer, sizeof(rx_buffer));
                if (status == HAL_OK) {
                    huart1.hdmarx->XferHalfCpltCallback = NULL;
                }
            }
            else if(rx_len == 1 && rx_buffer[0] == 0xFF)
            {
                consecutive_idle++;
                if (consecutive_idle >= 1)
                {
                    uint32_t total_received = Protocol_IAP_GetProgress();

                    if (total_received > 0)
                    {
                        /* 计算升级区的CRC */
                        uint32_t update_crc = Calculate_Image_CRC(UPDATE_REGION_BASE, total_received);
                        
                        /* 复制升级区代码到运行区 */
                        HAL_UART_Transmit(&huart1, (uint8_t *)"copy update to runapp...\r\n", strlen("copy update to runapp...\r\n"), 100);
                        if (Copy_Update_To_Runapp(total_received) != 0) {
                            g_config.page_count = 0;
                            Config_Write(&g_config);
                            UART1_in_update_mode = 0;
                            HAL_UART_Transmit(&huart1, (uint8_t *)"copy failed\r\n", strlen("copy failed\r\n"), 100);
                            return -4;
                        }
                        
                        /* 验证运行区的CRC */
                        uint32_t run_crc = Calculate_Image_CRC(RUNAPP_REGION_BASE, total_received);
                        if (run_crc != update_crc) {
                            g_config.page_count = 0;
                            Config_Write(&g_config);
                            UART1_in_update_mode = 0;
                            HAL_UART_Transmit(&huart1, (uint8_t *)"CRC check failed\r\n", strlen("CRC check failed\r\n"), 100);
                            return -5;
                        }
                        
                        /* 更新配置 */
                        g_config.firmware_CRC = run_crc;       // 运行区CRC
                        g_config.page_count = 0;          // 清除升级标志
                        Config_Write(&g_config);
                        
                        HAL_UART_Transmit(&huart1, (uint8_t *)"update success\r\n", strlen("update success\r\n"), 100);
                        UART1_in_update_mode = 0;
                        return 0;
                    }
                    else
                    {
                        g_config.page_count = 0;
                        Config_Write(&g_config);
                        UART1_in_update_mode = 0;
                        return -3;
                    }
                }
                else
                {
                    status = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer, sizeof(rx_buffer));
                    if (status == HAL_OK) {
                        huart1.hdmarx->XferHalfCpltCallback = NULL;
                    }
                }
            }
        }
        
        HAL_Delay(1);
    }
}

/************************************************************************/
int8_t IAP_Erase(void)
{
    uint8_t target = 0; // 固定升级区
    return Erase_Image(target) ? 0 : -1;
}

/************************************************************************/
/**
 * @brief Get current config (for external access)
 */
ImageConfig_t* IAP_GetConfig(void)
{
    return &g_config;
}

/**
 * @brief Confirm boot success (call from app or after verified boot)
 */
void IAP_ConfirmBoot(void)
{
    Confirm_Boot_Success(&g_config);
}
	
