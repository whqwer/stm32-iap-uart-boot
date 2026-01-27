/**
  ******************************************************************************
  * @file    IAP/inc/common.h 
  * @author  MCD Application Team
  * @version V3.3.0
  * @date    10/15/2010
  * @brief   This file provides all the headers of the common functions.
  ******************************************************************************
  * @copy
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2010 STMicroelectronics</center></h2>
  */ 

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _COMMON_H
#define _COMMON_H

/* Includes ------------------------------------------------------------------*/
#include "iap_config.h"
#include "stdio.h"
#include "string.h"
#include "stm32h503xx.h"
#include "stm32h5xx_hal.h"


/**
 * @brief Definition for COM port1, connected to USART1
 */ 
#define EVAL_COM1                        USART1
#define EVAL_COM1_TX_PIN                 GPIO_PIN_2
#define EVAL_COM1_TX_GPIO_PORT           GPIOA
#define EVAL_COM1_RX_PIN                 GPIO_PIN_1
#define EVAL_COM1_RX_GPIO_PORT           GPIOA
#define EVAL_COM1_IRQn                   USART1_IRQn

/**
 * @brief Definition for COM port2, connected to USART2
 */ 
//#define EVAL_COM2                        USART2
//#define EVAL_COM2_TX_PIN                 GPIO_PIN_2
//#define EVAL_COM2_TX_GPIO_PORT           GPIOA
//#define EVAL_COM2_RX_PIN                 GPIO_PIN_3
//#define EVAL_COM2_RX_GPIO_PORT           GPIOA
//#define EVAL_COM2_IRQn                   USART2_IRQn



/* Exported macro ------------------------------------------------------------*/
/* Common routines */

#define SerialPutString(x) Serial_PutString((uint8_t*)(x))
#define RESET                           0U
#define SET                             1U

/* Exported functions ------------------------------------------------------- */
extern void STM_EVAL_COMInit(UART_InitTypeDef* UART_InitStruct);
void Int2Str(uint8_t* str,int32_t intnum);
void Serial_PutString(uint8_t *s);
uint32_t FLASH_PagesMask(__IO uint32_t Size);
uint8_t EraseSomePages(__IO uint32_t size, uint8_t outPutCont);
extern void assert_failed(uint8_t* file, uint32_t line);
#endif  /* _COMMON_H */

/*******************(C)COPYRIGHT 2010 STMicroelectronics *****END OF FILE******/
