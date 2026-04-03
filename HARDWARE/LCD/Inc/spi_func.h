#ifndef _SPI_H_
#define _SPI_H_

#include "main.h"
//#define SPI1_DC_Pin GPIO_PIN_3
//#define SPI1_DC_GPIO_Port GPIOA
//#define SPI1_CS_Pin GPIO_PIN_4
//#define SPI1_CS_GPIO_Port GPIOA
//#define SPI1_RES_Pin GPIO_PIN_6
//#define SPI1_RES_GPIO_Port GPIOA

#define LCD_RES_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
#define LCD_RES_GPIO_PORT SPI1_RES_GPIO_Port
#define LCD_RES_GPIO_PIN SPI1_RES_Pin

#define LCD_DC_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
#define LCD_DC_GPIO_PORT SPI1_DC_GPIO_Port
#define LCD_DC_GPIO_PIN SPI1_DC_Pin

//#define LCD_CS_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
//#define LCD_CS_GPIO_PORT SPI1_CS_GPIO_Port
//#define LCD_CS_GPIO_PIN SPI1_CS_Pin


/* ����˿ڵ�ƽ״̬ */
//#define LCD_SCK_Clr() HAL_GPIO_WritePin(LCD_SCK_GPIO_PORT, LCD_SCK_GPIO_PIN, GPIO_PIN_RESET)
//#define LCD_SCK_Set() HAL_GPIO_WritePin(LCD_SCK_GPIO_PORT, LCD_SCK_GPIO_PIN, GPIO_PIN_SET)
//
//#define LCD_MOSI_Clr() HAL_GPIO_WritePin(LCD_MOSI_GPIO_PORT, LCD_MOSI_GPIO_PIN, GPIO_PIN_RESET)
//#define LCD_MOSI_Set() HAL_GPIO_WritePin(LCD_MOSI_GPIO_PORT, LCD_MOSI_GPIO_PIN, GPIO_PIN_SET)

#define LCD_RES_Clr() HAL_GPIO_WritePin(LCD_RES_GPIO_PORT, LCD_RES_GPIO_PIN, GPIO_PIN_RESET)
#define LCD_RES_Set() HAL_GPIO_WritePin(LCD_RES_GPIO_PORT, LCD_RES_GPIO_PIN, GPIO_PIN_SET)

#define LCD_DC_Clr() HAL_GPIO_WritePin(LCD_DC_GPIO_PORT, LCD_DC_GPIO_PIN, GPIO_PIN_RESET)
#define LCD_DC_Set() HAL_GPIO_WritePin(LCD_DC_GPIO_PORT, LCD_DC_GPIO_PIN, GPIO_PIN_SET)

//#define LCD_CS_Clr() HAL_GPIO_WritePin(LCD_CS_GPIO_PORT, LCD_CS_GPIO_PIN, GPIO_PIN_RESET)
//#define LCD_CS_Set() HAL_GPIO_WritePin(LCD_CS_GPIO_PORT, LCD_CS_GPIO_PIN, GPIO_PIN_SET)

/* �������� */
void LCD_GPIOInit(void);
void LCD_WR_Bus(uint8_t dat);
void LCD_WR_REG(uint8_t reg);
void LCD_WR_DATA8(uint8_t dat);
void LCD_WR_DATA(uint16_t dat);
void LCD_WR_Busbuf(uint8_t* dat, uint32_t len);
#endif
