/**
 * @file iap.c
 * @brief In-Application Programming (IAP) Implementation
 * 
 * This module handles:
 * - Jumping from bootloader to user application
 * - Protocol-based firmware update over UART
 * - Flash memory management for firmware storage
 * - Dual Image (A/B) management for fail-safe updates
 * 
 * Memory Layout:
 * - Bootloader: 0x08000000 - 0x08008000 (32KB)
 * - Config:     0x08006000 - 0x08008000 (8KB, last sector of bootloader)
 * - App A:      0x08008000 - 0x08014000 (48KB)
 * - App B:      0x08014000 - 0x08020000 (48KB)
 */

#include "iap_config.h"
#include "iap.h"
#include "iap_image.h"
#include "stmflash.h"
#include "protocol.h"
#include <stdlib.h>


pFunction Jump_To_Application;
uint32_t JumpAddress;

/* Global config for dual-image management */
static ImageConfig_t g_config;

/* Current update target address (used by protocol.c via extern) */
__attribute__((used)) uint32_t g_update_target_addr = IMAGE_A_BASE;




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
 * @details Initializes UART and loads config for dual-image management
 */
void IAP_Init(void)
{
    IAP_UART_Init();
    
    /* Load or initialize dual-image config */
    if (Config_Read(&g_config) != 0) {
        /* Config invalid, initialize with defaults */
        Config_Init();
        Config_Read(&g_config);
    }
}
/************************************************************************/
extern UART_HandleTypeDef huart1;

/**
 * @brief Jump from bootloader to user application
 * @details Performs dual-image selection and jumps to the valid application.
 *          
 * Jump Process:
 * 1. Select boot image (A or B) based on config and CRC verification
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
    /* Select boot image using dual-image logic */
    uint32_t boot_address = Select_Boot_Image(&g_config);
    
    /* Send boot_address to host PC via UART1 as string */
    const char *addr_str = (boot_address == 0x08008000) ? "0x08008000\r\n" : 
                           (boot_address == 0x08014000) ? "0x08014000\r\n" : "0x00000000\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t *)"Burn to ", strlen("Burn to "), 100);
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
 * @brief Execute protocol-based firmware update over UART with dual-image support
 * @details Complete firmware update process:
 *          
 * Update Flow:
 * 1. Select update target (inactive image)
 * 2. Mark update as started (updating=1)
 * 3. Erase target image region (48KB)
 * 4. Start DMA reception for incoming firmware data
 * 5. Main loop: receive and write firmware
 * 6. On success: Calculate CRC, switch to new image
 * 7. On failure: Keep old image active
 * 
 * @return 0 on success, -1 on erase failure, -2 on timeout, -3 on no data
 */
int8_t IAP_Update(void)
{
    uint32_t start_time;
    HAL_StatusTypeDef status;
    uint8_t consecutive_idle = 0;
    
    /* Reload config in case it changed */
    Config_Read(&g_config);
    
    /* Select update target (always the inactive image) */
    uint8_t target_image = Select_Update_Target(&g_config);
    g_update_target_addr = Get_Update_Address(&g_config);
    
    /* Set flag: Enter Update mode */
    UART1_in_update_mode = 1;
    
    /* 1. Mark update started (protection against power loss) */
    Update_Start(&g_config, target_image);
    
    /* 2. Initialize protocol layer */
    Protocol_IAP_Init();
    
    /* 3. Send target image info to host PC (0=A, 1=B) */
    /* Host should send app_a.bin for target=0, app_b.bin for target=1 */
     HAL_UART_Transmit(&huart1, (uint8_t *)"fireware start:", strlen("fireware start:"), 100);
    const char *target_addr_str = (target_image == 0) ? "0x08008000\r\n" : "0x08014000\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t*)target_addr_str, strlen(target_addr_str), 100);
    
    if (!Erase_Image(target_image))
    {
        Update_Failed(&g_config);
        UART1_in_update_mode = 0;
        return -1;
    }
    
    start_time = HAL_GetTick();
    
    /* 4. Start first DMA reception */
    status = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer, sizeof(rx_buffer));
    if (status != HAL_OK) {
        Update_Failed(&g_config);
        UART1_in_update_mode = 0;
        return -1;
    }
    huart1.hdmarx->XferHalfCpltCallback = NULL;

    /* 5. Main loop: wait and process data */
    while (1)
    {
        /* Check total timeout (30 seconds) */
        if ((HAL_GetTick() - start_time) > 30000)
        {
            Update_Failed(&g_config);
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
                        uint32_t new_crc = Calculate_Image_CRC(g_update_target_addr, total_received);
                        Update_Complete(&g_config, target_image, total_received, new_crc);
                        UART1_in_update_mode = 0;
                        return 0;
                    }
                    else
                    {
                        Update_Failed(&g_config);
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
    uint8_t target = Select_Update_Target(&g_config);
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
	

