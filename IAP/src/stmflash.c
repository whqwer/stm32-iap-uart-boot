#include "stmflash.h"
#include "iap_config.h"
/**
  * @brief  Read half words (16-bit data) of the specified address
  * @note   This function can be used for STM32H5 devices.
  * @param  faddr: The address to be read (the multiple of the address, which is 2)
  * @retval Value of specified address
  */
uint16_t STMFLASH_ReadHalfWord(uint32_t faddr)
{
	return *(uint16_t*)faddr;
}


/**
  * @brief  There is no check writing.
  * @note   This function can be used for STM32H5 devices.
  * @param  WriteAddr: The starting address to be written.
  * @param  pBuffer: The pointer to the data.
  * @param  NumToWrite:  The number of half words written
  * @retval None
  */

// ⚠️ Critical fix: Move all large buffers from stack to static storage to avoid stack overflow accessing 0x20008000
// Use static to ensure allocation in BSS segment with alignment
__attribute__((aligned(16))) static uint64_t qw_data_static[2];
static uint16_t temp_static[8];  // Temporary buffer also uses static storage

static void STMFLASH_Write_NoCheck(uint32_t WriteAddr,uint16_t *pBuffer,uint16_t NumToWrite)
{ 			  
	// ⚠️ Safety check: prevent NULL pointer access and overflow
	if (pBuffer == NULL || NumToWrite == 0) {
		return;
	}
	
	// ⚠️ Prevent address overflow (Flash range check)
	if (WriteAddr < STM32_FLASH_BASE || 
	    WriteAddr >= (STM32_FLASH_BASE + 0x20000) ||
	    WriteAddr + (NumToWrite * 2) > (STM32_FLASH_BASE + 0x20000)) {
		return;  // Address out of bounds, refuse write
	}
	
	uint16_t i;
	
	// ⚠️ Critical fix: Use static global qw_data_static to avoid stack overflow
	// Don't allocate on stack to prevent accessing 0x20008000 causing bus error
	
	// STM32H5 requires 128-bit (quadword) programming, 16-byte aligned
	// Process 8 halfwords (16 bytes) at a time
	for(i=0; i<NumToWrite; i+=8)
	{
		// ⚠️ Use static temp_static to avoid repeated stack allocation
		// Initialize with 0xFFFF padding
		for(uint16_t k=0; k<8; k++) {
			temp_static[k] = 0xFFFF;
		}
		
		// Copy available halfwords
		for(uint16_t j=0; j<8 && (i+j)<NumToWrite; j++) {
			temp_static[j] = pBuffer[i+j];
		}
		
		// Pack 8 halfwords into two 64-bit words (128-bit total)
		qw_data_static[0] = ((uint64_t)temp_static[0])       |
		                    ((uint64_t)temp_static[1] << 16) |
		                    ((uint64_t)temp_static[2] << 32) |
		                    ((uint64_t)temp_static[3] << 48);
		qw_data_static[1] = ((uint64_t)temp_static[4])       |
		                    ((uint64_t)temp_static[5] << 16) |
		                    ((uint64_t)temp_static[6] << 32) |
		                    ((uint64_t)temp_static[7] << 48);

		// ⚠️ Critical fix: Use static variable address to avoid illegal address access from stack overflow
		// Previously stack-based qw_data address might be 0x20008000 (exceeding RAM range)
		HAL_StatusTypeDef status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, 
		                                              WriteAddr, 
		                                              (uint32_t)(&qw_data_static[0]));

		        if (status != HAL_OK) {
		            return;
		        }
		WriteAddr += 16; // Advance by 16 bytes
	}
} 



uint16_t STMFLASH_BUF[PAGE_SIZE / 4];     // Flash sector buffer
//uint16_t STM32_FLASH_SIZE[PAGE_SIZE / 2];//Up to 4k bytes

/**
  * @brief  Write data to Flash memory from the specified address.
  * @note   This function handles sector-aligned writing with automatic erase-if-needed logic.
  *         For STM32H5 devices, the write address must be 16-byte aligned (quadword programming).
  *         The function performs read-modify-write operations when partial sector updates are needed.
  * 
  * @details Algorithm:
  *         1. Validates input parameters (NULL check, address range, alignment)
  *         2. Calculates sector position and offset within the sector
  *         3. Reads existing sector data into buffer
  *         4. Checks if erase is needed (any byte != 0xFFFF)
  *         5. If erase needed: erases sector, fills buffer with 0xFFFF, merges new data
  *         6. Writes the complete sector or remaining data
  *         7. Handles multi-sector writes by iterating through sectors
  * 
  * @param  WriteAddr: The starting Flash address to write (must be 16-byte aligned for STM32H5)
  * @param  pBuffer: Pointer to the source data buffer (array of 16-bit halfwords)
  * @param  NumToWrite: Number of halfwords (16-bit) to write
  * 
  * @retval None
  * 
  * @warning Interrupts are disabled during Flash operations to prevent conflicts.
  * @warning Function returns silently on error (NULL pointer, address out of range, misalignment).
  */
void STMFLASH_Write(uint32_t WriteAddr,uint16_t *pBuffer,uint16_t NumToWrite)
{
	// Safety check: prevent NULL pointer and invalid parameters
	if (pBuffer == NULL || NumToWrite == 0) {
		return;
	}
	
	// ⚠️ Address alignment check: STM32H5 Flash programming requires 16-byte alignment
	if ((WriteAddr & 0x0F) != 0) {
		// Address not aligned to 16-byte boundary
		return;
	}
	
	uint32_t secpos;	   // Sector index/position in Flash
	uint16_t secoff;	   // Offset within sector (in halfwords)
	uint16_t secremain;    // Remaining space in current sector (in halfwords)	   
 	uint16_t i = 0;        // Loop counter (must initialize to 0)
	uint32_t offaddr;      // Offset address (relative to Flash base)
	
	// Validate address is within valid Flash range (STM32H503: 128KB total)
	if(WriteAddr<STM32_FLASH_BASE || WriteAddr>=(STM32_FLASH_BASE+0x20000))return; // Invalid address
	
	__disable_irq();  // Disable interrupts during Flash operation
	HAL_FLASH_Unlock();  // Unlock Flash for writing
	
	offaddr=WriteAddr-STM32_FLASH_BASE;		// Calculate offset from Flash base
	secpos=offaddr/STM_SECTOR_SIZE;			// Calculate sector index (0~15 for STM32H503 with 8KB sectors)
	secoff=(offaddr%STM_SECTOR_SIZE)/2;		// Calculate offset within sector (in halfwords, 2 bytes each)
	secremain=STM_SECTOR_SIZE/2-secoff;		// Calculate remaining space in current sector (in halfwords)
	
	if(NumToWrite<=secremain)secremain=NumToWrite;  // Data fits within current sector
	while(1) 
	{	
		// Read entire sector into buffer for read-modify-write operation
		STMFLASH_Read(secpos*STM_SECTOR_SIZE+STM32_FLASH_BASE,STMFLASH_BUF,STM_SECTOR_SIZE/2);
		
		// Check if sector needs erasing (any non-0xFFFF byte means erase required)
		for(i=0;i<secremain;i++)
		{
			if(STMFLASH_BUF[secoff+i]!=0XFFFF)break;  // Found non-erased byte, need erase
		}
		
		 if(i<secremain)  // Erase is needed
		 {
			// Configure sector erase parameters
			FLASH_EraseInitTypeDef EraseInitStruct;
			uint32_t SectorError = 0;
			EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
			EraseInitStruct.Banks     = FLASH_BANK_1;
			EraseInitStruct.Sector    = secpos;   // Sector to erase (8KB sectors for STM32H503)
			EraseInitStruct.NbSectors = 1;        // Erase one sector at a time
			__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
//			 // ⚠️ Note: interrupts already disabled at function entry
//			    __disable_irq();

			    HAL_StatusTypeDef erase_status = HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);

//			    __enable_irq();

			    if (erase_status != HAL_OK) {
			        HAL_FLASH_Lock();
			        __enable_irq();
			        return;
			    }
			
			// After erase, fill buffer with 0xFFFF (erased state)
			// Note: memset fills bytes, so we must loop to fill halfwords correctly
			for(i=0; i<(PAGE_SIZE/4); i++) {
				STMFLASH_BUF[i] = 0xFFFF;
			}
			
			// Merge new data into buffer (read-modify-write)
			for(i=0;i<secremain;i++)
			{
				// Prevent buffer overflow
				if ((i+secoff) >= (PAGE_SIZE/4)) {
					break;  // Stop if exceeding buffer bounds
				}
				STMFLASH_BUF[i+secoff]=pBuffer[i];	  
			}
			
			// Write entire sector back to Flash
			STMFLASH_Write_NoCheck(secpos*STM_SECTOR_SIZE+STM32_FLASH_BASE,STMFLASH_BUF,STM_SECTOR_SIZE/2);

		 }else {
		 	// Sector already erased, write directly without erase operation
		 	STMFLASH_Write_NoCheck(WriteAddr,pBuffer,secremain);

		 }
		
		if(NumToWrite==secremain)break;  // All data written, exit loop
		else  // More data to write, continue to next sector
		{
			secpos++;				// Move to next sector		
			secoff=0;				// Reset offset to beginning of sector	 
		   	pBuffer+=secremain;  	// Advance source buffer pointer
		WriteAddr+=secremain*2;	// Advance write address (secremain is in halfwords, multiply by 2 for bytes)	   
		   	NumToWrite-=secremain;	// Decrease remaining count
		   	
		   	// Calculate how much to write in next iteration
			if(NumToWrite>(STM_SECTOR_SIZE/2))secremain=STM_SECTOR_SIZE/2;  // Next sector will be full
			else secremain=NumToWrite;  // Last partial sector
		}	 
	};	
	
	HAL_FLASH_Lock();  // Lock Flash after operation
	__enable_irq();    // Re-enable interrupts
}

/**
  * @brief  Read data from Flash memory starting at the specified address.
  * @note   This function performs sequential halfword (16-bit) reads from Flash.
  *         Compatible with all STM32 devices including STM32F10x and STM32H5 series.
  *         No alignment restrictions for read operations.
  * 
  * @details Algorithm:
  *         1. Iterates through the specified number of halfwords
  *         2. Reads each halfword (2 bytes) using STMFLASH_ReadHalfWord()
  *         3. Stores the value in the destination buffer
  *         4. Advances read address by 2 bytes per iteration
  * 
  * @param  ReadAddr: Starting Flash address to read from (any valid Flash address)
  * @param  pBuffer: Pointer to the destination buffer (array of 16-bit halfwords)
  * @param  NumToRead: Number of halfwords (16-bit) to read
  * 
  * @retval None
  * 
  * @warning No boundary checking is performed. Ensure ReadAddr and buffer size are valid.
  * @note   This is a simple memory-mapped read operation, safe to call anytime.
  */
void STMFLASH_Read(uint32_t ReadAddr,uint16_t *pBuffer,uint16_t NumToRead)
{
	uint16_t i;
	for(i=0;i<NumToRead;i++)
	{
		pBuffer[i]=STMFLASH_ReadHalfWord(ReadAddr);// Read 2 bytes (1 halfword)
		ReadAddr+=2;// Advance address by 2 bytes	
	}
}
					















