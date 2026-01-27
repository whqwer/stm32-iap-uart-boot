#ifndef __STMFLASH_H__
#define __STMFLASH_H__
#include "stm32h5xx_hal.h"
#include "stm32h5xx_hal_flash.h"
#include "common.h"


extern uint16_t STMFLASH_ReadHalfWord(uint32_t faddr);		 //Read halfword
extern void STMFLASH_Write(uint32_t addr,uint16_t *buffer,uint16_t count);		//Write specified length of data from specified address
extern void STMFLASH_Read(uint32_t ReadAddr,uint16_t *pBuffer,uint16_t NumToRead);   		//Read specified length of data from specified address
#endif

















