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
static void STMFLASH_Write_NoCheck(uint32_t WriteAddr,uint16_t *pBuffer,uint16_t NumToWrite)
{ 			  
	uint16_t i;
	uint64_t qw_data[2]; // 128-bit = 2 x 64-bit
	// STM32H5 requires 128-bit (quadword) programming, 16-byte aligned
	// Process 8 halfwords (16 bytes) at a time
	for(i=0; i<NumToWrite; i+=8)
	{
		// Prepare temp buffer with 0xFFFF padding
		uint16_t temp[8] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
		// Copy available halfwords
		for(uint16_t j=0; j<8 && (i+j)<NumToWrite; j++) {
			temp[j] = pBuffer[i+j];
		}
		// Pack 8 halfwords into two 64-bit words (128-bit total)
		qw_data[0] = ((uint64_t)temp[0])       |
		             ((uint64_t)temp[1] << 16) |
		             ((uint64_t)temp[2] << 32) |
		             ((uint64_t)temp[3] << 48);
		qw_data[1] = ((uint64_t)temp[4])       |
		             ((uint64_t)temp[5] << 16) |
		             ((uint64_t)temp[6] << 32) |
		             ((uint64_t)temp[7] << 48);
		
		if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, WriteAddr, (uint32_t)qw_data) != HAL_OK) {
			return; // Write failed
		}
		WriteAddr += 16; // Advance by 16 bytes
	}
} 

uint16_t STMFLASH_BUF[PAGE_SIZE / 2];     // Flash sector buffer
uint16_t STM32_FLASH_SIZE[PAGE_SIZE / 2];//Up to 4k bytes

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
  uint32_t secpos;	   //������ַ
	uint16_t secoff;	   //������ƫ�Ƶ�ַ(16λ�ּ���)
	uint16_t secremain; //������ʣ����?(16λ�ּ���)	   
 	uint16_t i;    
	uint32_t offaddr;   //ȥ��0X08000000��ĵ��?
	// Check if address is within valid flash range (STM32H503: 128KB total)
	if(WriteAddr<STM32_FLASH_BASE || WriteAddr>=(STM32_FLASH_BASE+0x20000))return;//�Ƿ���ַ
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
			HAL_FLASH_Unlock();
			FLASH_EraseInitTypeDef EraseInitStruct;
			uint32_t SectorError = 0;
			EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
			EraseInitStruct.Banks     = FLASH_BANK_1;
			EraseInitStruct.Sector    = secpos;   // �� 8KB ���������?
			EraseInitStruct.NbSectors = 1;
			__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
			if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
				HAL_FLASH_Lock();
				return; // ����ʧ��ֱ�ӷ��أ��ɸ�����Ҫ��ӡ SectorError
			}
			// �����󽫻���������?0xFFFF������Ѿ���������д��?
			memset(STMFLASH_BUF, 0xFFFF, sizeof(STMFLASH_BUF));
			for(i=0;i<secremain;i++)//����
			{
				STMFLASH_BUF[i+secoff]=pBuffer[i];	  
			}
			STMFLASH_Write_NoCheck(secpos*STM_SECTOR_SIZE+STM32_FLASH_BASE,STMFLASH_BUF,STM_SECTOR_SIZE/2);//д����������

		}else {
			HAL_FLASH_Unlock();
			STMFLASH_Write_NoCheck(WriteAddr,pBuffer,secremain);//д�Ѿ������˵�,ֱ��д������ʣ������.

		}				   
		if(NumToWrite==secremain)break;//д�������??
		else//д��δ����
		{
			secpos++;				//������ַ��1		
			secoff=0;				//ƫ��λ��Ϊ0 	 
		   	pBuffer+=secremain;  	//ָ��ƫ��
			WriteAddr+=secremain;	//д��ַƫ��	   
		   	NumToWrite-=secremain;	//�ֽ�(16λ)���ݼ�
			if(NumToWrite>(STM_SECTOR_SIZE/2))secremain=STM_SECTOR_SIZE/2;//��һ����������д����
			else secremain=NumToWrite;//��һ����������д����
		}	 
	};	
	HAL_FLASH_Lock();//����
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
					















