/**
 * @file iap.c
 * @brief In-Application Programming (IAP) Implementation
 * 
 * This module handles:
 * - Jumping from bootloader to user application
 * - Protocol-based firmware update over UART
 * - Flash memory management for firmware storage
 * 
 * Memory Layout:
 * - Bootloader: 0x08000000 - 0x0800C000 (48KB)
 * - Application: 0x0800C000 - 0x08020000 (80KB)
 */

#include "iap_config.h"
#include "iap.h"
#include "stmflash.h"
#include "protocol.h"
#include <stdlib.h>

pFunction Jump_To_Application;
uint32_t JumpAddress;




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
 * @details Initializes UART and prepares for firmware update or application jump
 */
void IAP_Init(void)
{
    IAP_UART_Init();
#if (USE_BKP_SAVE_FLAG == 1)
	// BKP is not supported in this implementation
#endif
}
/************************************************************************/
extern UART_HandleTypeDef huart1;

/**
 * @brief Jump from bootloader to user application
 * @details Performs a complete system reset and jumps to the user application.
 *          
 * Jump Process:
 * 1. Validates application stack pointer in SRAM range
 * 2. Disables all interrupts and peripherals
 * 3. Resets SysTick timer
 * 4. Clears pending interrupts
 * 5. Relocates vector table to application address
 * 6. Invalidates and disables caches (STM32H5 specific)
 * 7. Sets stack pointer to application's initial SP
 * 8. Jumps to application's reset handler
 * 
 * @return 0 if successful (should never return), -1 if stack pointer invalid
 * @note This function should never return if application is valid
 */
int8_t IAP_RunApp(void)
{
		// Read application's initial stack pointer
	uint32_t sp = (*(__IO uint32_t*)ApplicationAddress);
	
	// Validate stack pointer (STM32H503 SRAM: 0x20000000-0x20008000, 32KB)
	// Stack pointer should be between SRAM start and end address (inclusive)
	if (sp >= 0x20000000 && sp <= 0x20008000)
	{   
		SerialPutString("\r\n Run to app.\r\n");
		
		// 1. Disable global interrupts
		__disable_irq();
		
		// 2. De-initialize peripherals
		HAL_UART_MspDeInit(&huart1);
		HAL_DeInit();
		
		// 3. Disable SysTick
		SysTick->CTRL = 0;
		SysTick->LOAD = 0;
		SysTick->VAL = 0;
		
		// 4. Clear all pending interrupts
		for (uint8_t i = 0; i < 8; i++)
		{
			NVIC->ICER[i] = 0xFFFFFFFF;
			NVIC->ICPR[i] = 0xFFFFFFFF;
		}
		
		// 5. Set vector table offset to application address
		SCB->VTOR = ApplicationAddress;
		
		// 6. STM32H5 cache handling - clear and disable cache
		#if (__ICACHE_PRESENT == 1)
		SCB_InvalidateICache();
		SCB_DisableICache();
		#endif
		#if (__DCACHE_PRESENT == 1)
		SCB_CleanInvalidateDCache();
		SCB_DisableDCache();
		#endif
		
		// 7. Set main stack pointer
		__set_MSP(*(__IO uint32_t*) ApplicationAddress);
		
		// 8. Get reset vector address and jump
		JumpAddress = *(__IO uint32_t*) (ApplicationAddress + 4);
		Jump_To_Application = (pFunction) JumpAddress;
		
		// 9. Jump to application
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
#define MAX_FRAME_SIZE    1024*10
// UART receive buffer (receive up to 1024*10 bytes at once)
extern uint8_t rx_buffer[MAX_FRAME_SIZE];

// Protocol-based firmware update (new version)
extern uint8_t UART1_in_update_mode;  // Declare external variable
extern uint8_t UART1_Complete_flag;
extern uint16_t rx_len;

/**
 * @brief Execute protocol-based firmware update over UART
 * @details Complete firmware update process:
 *          
 * Update Flow:
 * 1. Initialize protocol layer (reset UART, clear errors)
 * 2. Erase application Flash region (80KB)
 * 3. Start DMA reception for incoming firmware data
 * 4. Main loop:
 *    - Wait for UART idle event (frame complete)
 *    - Parse received protocol frames
 *    - Extract firmware data and write to Flash
 *    - Send ACK/NACK responses
 * 5. Detect end of transmission (consecutive idle events)
 * 6. Verify total bytes received
 * 
 * @return 0 on success, -1 on erase failure, -2 on timeout, -3 on no data
 * @note This is a blocking function with 30-second timeout
 * @note Uses DMA for efficient UART reception
 */
int8_t IAP_Update(void)
{
    uint32_t start_time;
    HAL_StatusTypeDef status;
    uint8_t consecutive_idle = 0;  // Consecutive idle count
    
    // Set flag: Enter Update mode
    UART1_in_update_mode = 1;
    
    // 1. Initialize protocol layer
    Protocol_IAP_Init();
    
    // 2. Erase Flash
    SerialPutString("\r\n=== IAP Update Mode ===\r\n");
    SerialPutString("Erasing flash...\r\n");
    if (!EraseSomePages(FLASH_IMAGE_SIZE, 1))
    {
        SerialPutString("Erase failed!\r\n");
        UART1_in_update_mode = 0;
        return -1;
    }
    
    start_time = HAL_GetTick();
    
    // ✅ Key modification 1: Start first DMA reception outside the loop
    status = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer, sizeof(rx_buffer));
    if (status != HAL_OK) {
        SerialPutString("DMA start failed!\r\n");
        UART1_in_update_mode = 0;
        return -1;
    }
    huart1.hdmarx->XferHalfCpltCallback = NULL;
    SerialPutString("DMA started, waiting for data...\r\n");

    // ✅ Key modification 2: Main loop only waits and processes data
    while (1)
    {
        // Check total timeout (30 seconds)
        if ((HAL_GetTick() - start_time) > 30000)
        {
            SerialPutString("\r\n=== Update Timeout (30s)! ===\r\n");
            UART1_in_update_mode = 0;
            return -2;
        }
        
        // ✅ Key modification 3: Only check flag, not status
        if(UART1_Complete_flag)
        {
            UART1_Complete_flag = 0;
            
            // ✅ Read DMA counter in main loop, DMA is fully stable at this point
            uint16_t remaining = (uint16_t)__HAL_DMA_GET_COUNTER(huart1.hdmarx);
            rx_len = sizeof(rx_buffer) - remaining;  // Actual bytes received accurately
            
            // ✅ Key modification 4: Judge data status based on rx_len
            if (rx_len > 1)
            {
                
                // Process data
                Protocol_Receive(rx_buffer, rx_len);
                memset(rx_buffer, 0, MAX_FRAME_SIZE);
                
                // Reset idle count (received data means transmission is ongoing)
                consecutive_idle = 0;

                // ✅ Key modification 5: Restart DMA after processing
                // ✅ This is the safest place!
                HAL_UART_AbortReceive(&huart1);  // Completely stop and reset DMA
                status = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer, sizeof(rx_buffer));
                if (status == HAL_OK) {
                    huart1.hdmarx->XferHalfCpltCallback = NULL;
                }
            }
            else if(rx_len == 1 && rx_buffer[0] == 0xFF)
            {
                // ✅ Key modification 6: This is the real "transmission end" judgment logic
                consecutive_idle++;

                SerialPutString("\r\n[IDLE] Bus idle detected (");
                uint8_t count_str[4];
                Int2Str(count_str, consecutive_idle);
                SerialPutString(count_str);
                SerialPutString("/2)\r\n");

                // Consecutive 2 idle with no data, consider transmission complete
                if (consecutive_idle >= 1)
                {
                    uint32_t total_received = Protocol_IAP_GetProgress();

                    if (total_received > 0)
                    {
                        // ✅ Transmission successful
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
                        // No data received when ended
                        SerialPutString("\r\n=== No data received ===\r\n");
                        UART1_in_update_mode = 0;
                        return -3;
                    }
                }
                else
                {
                    // First idle, may be packet interval, continue waiting
                    SerialPutString("Waiting for more data...\r\n");

                    // Restart DMA
                    status = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer, sizeof(rx_buffer));
                    if (status == HAL_OK) {
                        huart1.hdmarx->XferHalfCpltCallback = NULL;
                    }
                }
            }
        }
        
        // Appropriate delay to avoid high CPU usage
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
	

