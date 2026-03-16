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

/* Expected total page count for update completion check */
__attribute__((used)) uint16_t g_expected_page_count = 0;




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
        /* Config无效（magic不匹配）：说明标志区被擦除但未写完（断电场景）
         * 重新初始化：尝试自动探测已有固件，若有则记录CRC，page_count=0（直接运行） */
//        HAL_UART_Transmit(&huart1, (uint8_t*)"Config invalid, reinit...\r\n", 27, 100);
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

	uint32_t boot_address = RUNAPP_REGION_BASE;

    /* Read application's initial stack pointer */
    uint32_t sp = (*(__IO uint32_t*)boot_address);

    /* Validate stack pointer (STM32H503 SRAM: 0x20000000-0x20008000, 32KB) */
    if (sp >= 0x20000000 && sp <= 0x20008000)
    {
//        HAL_Delay(10);

        /* 1. Disable global interrupts */
        __disable_irq();

        /* 2. Stop all DMA transfers to prevent spurious interrupts */
        // if (huart1.hdmarx != NULL) {
        //     HAL_DMA_Abort(huart1.hdmarx);
        // }
        // if (huart1.hdmatx != NULL) {
        //     HAL_DMA_Abort(huart1.hdmatx);
        // }
        
        // /* 3. De-initialize peripherals */
        // HAL_UART_DeInit(&huart1);
        HAL_UART_MspDeInit(&huart1);
        HAL_DeInit();

        /* 4. Disable SysTick */
        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL = 0;

        /* 5. Clear all pending interrupts */
        for (uint8_t i = 0; i < 8; i++)
        {
            NVIC->ICER[i] = 0xFFFFFFFF;
            NVIC->ICPR[i] = 0xFFFFFFFF;
        }

        /* 6. Set vector table offset to application address */
        SCB->VTOR = boot_address;
        __DSB();  // Data Synchronization Barrier
        __ISB();  // Instruction Synchronization Barrier

        /* 7. STM32H5 cache handling - clear and disable cache */
        #if (__ICACHE_PRESENT == 1)
        SCB_InvalidateICache();
        SCB_DisableICache();
        #endif
        #if (__DCACHE_PRESENT == 1)
        SCB_CleanInvalidateDCache();
        SCB_DisableDCache();
        #endif

        /* 8. Set main stack pointer */
        __set_MSP(*(__IO uint32_t*) boot_address);

        /* 9. Get reset vector address and jump */
        JumpAddress = *(__IO uint32_t*) (boot_address + 4);
        Jump_To_Application = (pFunction) JumpAddress;

        /* 10. Jump to application */
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
 * @brief Copy the update region code to the run region
 * @param page_count Number of pages
 * @return 0=success, -1=failure
 */
static int8_t Copy_Update_To_Runapp(uint16_t page_count)
{
    uint32_t update_addr = UPDATE_REGION_BASE;
    uint32_t runapp_addr = RUNAPP_REGION_BASE;
    uint16_t remaining_pages = page_count;
    
    /* 4. Erase the run region */
       uint8_t target_image = 1; // 1=run region
       if (!Erase_Image(target_image))
       {
           g_config.page_count = 0;
           Config_Write(&g_config);
           UART1_in_update_mode = 0;
           return -1;
       }

    while (remaining_pages > 0)
    {
        uint16_t num_halfwords = PAGE_SIZE / 2;
        
        STMFLASH_Write(runapp_addr, (uint16_t*)update_addr, num_halfwords);
        
        runapp_addr += PAGE_SIZE;
        update_addr += PAGE_SIZE;
        remaining_pages--;
    }
    
    return 0;
}

int8_t IAP_Update(void)
{
    uint32_t start_time;
    HAL_StatusTypeDef status;
    
    /* Always update to the update region */
    g_update_target_addr = UPDATE_REGION_BASE;
    
    /* Set expected page count from config */
    g_expected_page_count = g_config.page_count;
    
    /* Set flag: Enter Update mode */
    UART1_in_update_mode = 1;
    
    
    /* 2. Initialize protocol layer */
    Protocol_IAP_Init();
    
    /* 3. Send target image info to host PC */
//    HAL_UART_Transmit(&huart1, (uint8_t *)"fireware update start at:", strlen("fireware update start at:"), 100);
//    HAL_UART_Transmit(&huart1, (uint8_t*)"0x08008000\r\n", strlen("0x08008000\r\n"), 100);
    
    /* 4. Erase the update region */
    uint8_t target_image = 0; // 0=update region
    if (!Erase_Image(target_image))
    {
//        g_config.page_count = 0;
//        Config_Write(&g_config);
//        UART1_in_update_mode = 0;
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
    	// Feed the watchdog to prevent reset
    	IWDG->KR = 0xAAAA;
        /* Check total timeout (30 seconds) */
        if ((HAL_GetTick() - start_time) > 30000)
        {
//            g_config.page_count = 0;
//            Config_Write(&g_config);
//            UART1_in_update_mode = 0;
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
                
                /* Check if last page received using page_index */
                volatile uint16_t current_page_index = Protocol_IAP_GetCurrentPageIndex();
                uint32_t total_received = Protocol_IAP_GetProgress();
                /* total_received>0 guard: 若尚未写入任何数据（如首包帧CRC错误），
                 * 不触发CRC检查，避免stale的current_page_index误触发提前返回。 */
                if (total_received > 0 && g_expected_page_count > 0 && current_page_index >= g_expected_page_count - 1)
                {

                    /* 将实际写入 flash 的固件 CRC 与主机发来的期望 CRC 比对。
                     * g_config.firmware_CRC 由 APP 从 enter-upgrade 数据包中解析
                     * 并写入 Config 区，代表主机侧对固件文件计算的 CRC32。
                     * 只有两者一致，才说明固件完整传输，才清除升级标志并跳转。
                     *
                     * 原代码错误：用 Calculate_Image_CRC(RUNAPP_REGION_BASE) 与
                     * Calculate_Image_CRC(UPDATE_REGION_BASE) 比对，但两个宏都定
                     * 义为 0x08008000（同一地址），永远相等，校验完全失效：即使固
                     * 件损坏，page_count 也会被清 0，导致跳转到无效固件后死机。   */
                    uint32_t actual_crc = Calculate_Image_CRC(UPDATE_REGION_BASE, total_received);

                    if (actual_crc != g_config.firmware_CRC) {
//                        HAL_UART_Transmit(&huart1, (uint8_t *)"CRC check failed\r\n",
//                                          strlen("CRC check failed\r\n"), 100);
                        return -5;  /* 固件损坏，不清除标志，等主机重传 */
                    }

                    /* CRC 验证通过：固件完整，firmware_CRC 保持不变 */
                    g_config.page_count = 0;               // 清除升级标志，下次直接运行
                    if (Config_Write(&g_config) != 0) {
                        /* Config写入失败（极少发生），标志区可能被擦除
                         * 此处固件已正确写入，直接跳转运行，下次启动Config_Init会重建 */
//                        HAL_UART_Transmit(&huart1, (uint8_t *)"Config write failed, jump anyway\r\n", 35, 100);
                        return -4;
                    }
//                    HAL_UART_Transmit(&huart1, (uint8_t *)"update success\r\n", 16, 100);
                    UART1_in_update_mode = 0;
                    return 0;
                }

                /* Restart DMA after processing */
                HAL_UART_AbortReceive(&huart1);
                status = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer, sizeof(rx_buffer));
                if (status == HAL_OK) {
                    huart1.hdmarx->XferHalfCpltCallback = NULL;
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
        
        HAL_Delay(1);
    }
}

/************************************************************************/
int8_t IAP_Erase(void)
{
    uint8_t target = 0; // Always update region
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
	
