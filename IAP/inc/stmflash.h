#ifndef __STMFLASH_H__
#define __STMFLASH_H__
#include "stm32h5xx_hal.h"
#include "stm32h5xx_hal_flash.h"
#include "common.h"


extern uint16_t STMFLASH_ReadHalfWord(uint32_t faddr);		 //读出半字  
extern void STMFLASH_Write(uint32_t addr,uint16_t *buffer,uint16_t count);		//从指定地址开始写入指定长度的数据
extern void STMFLASH_Read(uint32_t ReadAddr,uint16_t *pBuffer,uint16_t NumToRead);   		//从指定地址开始读出指定长度的数据
#endif

















