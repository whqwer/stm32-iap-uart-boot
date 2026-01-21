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

// ⚠️ 关键修复：将所有大缓冲区从栈移到静态存储，避免栈溢出导致访问 0x20008000
// 使用 static 确保在 BSS 段分配，且对齐
__attribute__((aligned(16))) static uint64_t qw_data_static[2];
static uint16_t temp_static[8];  // 临时缓冲区也使用静态存储

static void STMFLASH_Write_NoCheck(uint32_t WriteAddr,uint16_t *pBuffer,uint16_t NumToWrite)
{ 			  
	// ⚠️ 安全检查：防止 NULL 指针访问和越界
	if (pBuffer == NULL || NumToWrite == 0) {
		return;
	}
	
	// ⚠️ 防止地址溢出（Flash 范围检查）
	if (WriteAddr < STM32_FLASH_BASE || 
	    WriteAddr >= (STM32_FLASH_BASE + 0x20000) ||
	    WriteAddr + (NumToWrite * 2) > (STM32_FLASH_BASE + 0x20000)) {
		return;  // 地址越界，拒绝写入
	}
	
	uint16_t i;
	
	// ⚠️ 关键修复：使用静态全局 qw_data_static 避免栈溢出
	// 不在栈上分配，防止访问 0x20008000 导致总线错误
	
	// STM32H5 requires 128-bit (quadword) programming, 16-byte aligned
	// Process 8 halfwords (16 bytes) at a time
	for(i=0; i<NumToWrite; i+=8)
	{
		// ⚠️ 使用静态 temp_static，避免在栈上重复分配
		// 初始化为 0xFFFF padding
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

		// ⚠️ 关键修复：使用静态变量地址，避免栈溢出导致的非法地址访问
		// 之前栈上的 qw_data 地址可能是 0x20008000（超出 RAM 范围）
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
  * @brief  Write data from the specified address to the specified length.
  * @note   This function can be used for STM32H5 devices.
  * @param  addr: The starting address to be written.(The address must be a multiple of two)
  * @param  buffer: The pointer to the data.
  * @param  count:  The number of half words written
  * @retval None
  */
void STMFLASH_Write(uint32_t WriteAddr,uint16_t *pBuffer,uint16_t NumToWrite)
{
	// 安全检查：防止 NULL 指针和无效参数
	if (pBuffer == NULL || NumToWrite == 0) {
		return;
	}
	
	// ⚠️ 地址对齐检查：STM32H5 Flash 编程地址必须 16 字节对齐
	if ((WriteAddr & 0x0F) != 0) {
		// 地址未对齐到 16 字节边界
		return;
	}
	
	uint32_t secpos;	   //������ַ
	uint16_t secoff;	   //������ƫ�Ƶ�ַ(16λ�ּ���)
	uint16_t secremain; //������ʣ����?(16λ�ּ���)	   
 	uint16_t i = 0;    // ⚠️ 必须初始化为0！防止使用随机值
	uint32_t offaddr;   //ȥ��0X08000000��ĵ��?
	// Check if address is within valid flash range (STM32H503: 128KB total)
	if(WriteAddr<STM32_FLASH_BASE || WriteAddr>=(STM32_FLASH_BASE+0x20000))return;//�Ƿ���ַ
	 __disable_irq();
	HAL_FLASH_Unlock();						//����
	offaddr=WriteAddr-STM32_FLASH_BASE;		//ʵ��ƫ�Ƶ�ַ.
	secpos=offaddr/STM_SECTOR_SIZE;			//������ַ  0~127 for STM32F103RBT6
	secoff=(offaddr%STM_SECTOR_SIZE)/2;		//�������ڵ�ƫ��(2���ֽ�Ϊ������λ.)
	secremain=STM_SECTOR_SIZE/2-secoff;		//����ʣ��ռ��С   
	if(NumToWrite<=secremain)secremain=NumToWrite;//�����ڸ�������Χ
	while(1) 
	{	
		STMFLASH_Read(secpos*STM_SECTOR_SIZE+STM32_FLASH_BASE,STMFLASH_BUF,STM_SECTOR_SIZE/2);//������������������
		for(i=0;i<secremain;i++)//У������
		{
			if(STMFLASH_BUF[secoff+i]!=0XFFFF)break;//��Ҫ����
		}
		 if(i<secremain)//��Ҫ����
		 {
			FLASH_EraseInitTypeDef EraseInitStruct;
			uint32_t SectorError = 0;
			EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
			EraseInitStruct.Banks     = FLASH_BANK_1;
			EraseInitStruct.Sector    = secpos;   // �� 8KB ���������?
			EraseInitStruct.NbSectors = 1;
			__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
//			 // ⚠️ 擦除前禁用中断（擦除时间更长，更危险！）
//			    __disable_irq();

			    HAL_StatusTypeDef erase_status = HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);

//			    __enable_irq();

			    if (erase_status != HAL_OK) {
			        HAL_FLASH_Lock();
			        __enable_irq();
			        return;
			    }
			// �����󽫻���������?0xFFFF������Ѿ���������д��?
			// 修复：正确填充 0xFFFF（memset 只能填充字节，需要循环填充半字）
			for(i=0; i<(PAGE_SIZE/4); i++) {
				STMFLASH_BUF[i] = 0xFFFF;
			}
			// ⚠️ 关键检查：防止数组越界访问
			for(i=0;i<secremain;i++)//����
			{
				// 确保不会超出 STMFLASH_BUF 的范围
				if ((i+secoff) >= (PAGE_SIZE/4)) {
					break;  // 防止越界
				}
				STMFLASH_BUF[i+secoff]=pBuffer[i];	  
			}
			STMFLASH_Write_NoCheck(secpos*STM_SECTOR_SIZE+STM32_FLASH_BASE,STMFLASH_BUF,STM_SECTOR_SIZE/2);//д����������

		 }else {
		 	STMFLASH_Write_NoCheck(WriteAddr,pBuffer,secremain);//д�Ѿ������˵�,ֱ��д������ʣ������.

		 }
		if(NumToWrite==secremain)break;//д�������??
		else//д��δ����
		{
			secpos++;				//������ַ��1		
			secoff=0;				//ƫ��λ��Ϊ0 	 
		   	pBuffer+=secremain;  	//ָ��ƫ��
		WriteAddr+=secremain*2;	//д��ַƫ��(修正：secremain是半字数，地址需要*2)	   
		   	NumToWrite-=secremain;	//�ֽ�(16λ)���ݼ�
			if(NumToWrite>(STM_SECTOR_SIZE/2))secremain=STM_SECTOR_SIZE/2;//��һ����������д����
			else secremain=NumToWrite;//��һ����������д����
		}	 
	};	
	HAL_FLASH_Lock();//����
	__enable_irq();
}

/**
  * @brief  Start reading the specified data from the specified address.
  * @note   This function can be used for all STM32F10x devices.
  * @param  ReadAddr: Start addr
  * @param  pBuffer: The pointer to the data.
  * @param  NumToWrite:  The number of half words written(16bit)
  * @retval None
  */
void STMFLASH_Read(uint32_t ReadAddr,uint16_t *pBuffer,uint16_t NumToRead)
{
	uint16_t i;
	for(i=0;i<NumToRead;i++)
	{
		pBuffer[i]=STMFLASH_ReadHalfWord(ReadAddr);//��ȡ2�ֽ�����.
		ReadAddr+=2;//ƫ��2�ֽ�����.	
	}
}
					















