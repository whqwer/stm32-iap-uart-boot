#include "spi_func.h"
#include <string.h>
#include "main.h"
#include "spi.h"
/**
 * @brief       GPIO initialization for LCD control pins
 * @param       None
 * @retval      None
 */
//#define LCD_RES_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
//#define LCD_RES_GPIO_PORT SPI_RES_GPIO_Port
//#define LCD_RES_GPIO_PIN SPI_RES_Pin
//
//#define LCD_DC_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
//#define LCD_DC_GPIO_PORT SPI_DC_GPIO_Port
//#define LCD_DC_GPIO_PIN SPI_DC_Pin
//
//#define LCD_CS_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
//#define LCD_CS_GPIO_PORT SPI_CS_GPIO_Port
//#define LCD_CS_GPIO_PIN SPI_CS_Pin
void LCD_GPIOInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
//    LCD_SCK_CLK_ENABLE();
//    LCD_MOSI_CLK_ENABLE();
    LCD_RES_CLK_ENABLE();
    LCD_DC_CLK_ENABLE();
//    LCD_CS_CLK_ENABLE();
    
    GPIO_InitStructure.Pin=LCD_DC_GPIO_PIN;
    GPIO_InitStructure.Mode=GPIO_MODE_OUTPUT_PP;
    GPIO_InitStructure.Speed=GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LCD_DC_GPIO_PORT,&GPIO_InitStructure);

//    GPIO_InitStructure.Pin=LCD_CS_GPIO_PIN;
//    GPIO_InitStructure.Mode=GPIO_MODE_OUTPUT_PP;
//    GPIO_InitStructure.Speed=GPIO_SPEED_FREQ_HIGH;
//    HAL_GPIO_Init(LCD_CS_GPIO_PORT,&GPIO_InitStructure);

    GPIO_InitStructure.Pin=LCD_RES_GPIO_PIN;
    GPIO_InitStructure.Mode=GPIO_MODE_OUTPUT_PP;
    GPIO_InitStructure.Speed=GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LCD_RES_GPIO_PORT,&GPIO_InitStructure);

//    GPIO_InitStructure.Pin=SPI1_MOSI_Pin;
//    GPIO_InitStructure.Mode=GPIO_MODE_OUTPUT_PP;
//    GPIO_InitStructure.Speed=GPIO_SPEED_FREQ_HIGH;
//    HAL_GPIO_Init(SPI1_MOSI_GPIO_Port,&GPIO_InitStructure);
//
//    GPIO_InitStructure.Pin=SPI1_SCK_Pin;
//    GPIO_InitStructure.Mode=GPIO_MODE_OUTPUT_PP;
//    GPIO_InitStructure.Speed=GPIO_SPEED_FREQ_HIGH;
//    HAL_GPIO_Init(SPI1_SCK_GPIO_Port,&GPIO_InitStructure);
}

/**
 * @brief       Send one byte data via SPI (DMA mode)
 * @param       dat: Byte data to send
 * @retval      None
 */
void LCD_WR_Bus(uint8_t dat)
{
//    LCD_CS_Clr();
    HAL_SPI_Transmit_DMA(&hspi1, &dat, 1);
//    while(HAL_SPI_GetState(&hspi1)!=HAL_SPI_STATE_READY); // Wait for completion
//    HAL_SPI_Transmit(&hspi1, &dat, 1, 1000);
//    LCD_CS_Set();
}

/**
 * @brief       Write register command to LCD
 * @param       reg: Command to write
 * @retval      None
 * @note        DC==0 for command mode
 */
void LCD_WR_REG(uint8_t reg)
{
    LCD_DC_Clr();
    LCD_WR_Bus(reg);
//    LCD_DC_Set();
}

/**
 * @brief       Write one byte data to LCD
 * @param       dat: Data to write
 * @retval      None
 * @note        DC==1 for data mode
 */
void LCD_WR_DATA8(uint8_t dat)
{
    LCD_DC_Set();
    LCD_WR_Bus(dat);
//    LCD_DC_Set();
}

/**
 * @brief       Write one half-word (16-bit) data to LCD
 * @param       dat: Data to write
 * @retval      None
 */
void LCD_WR_DATA(uint16_t dat)
{
    LCD_DC_Set();
    LCD_WR_Bus(dat >> 8);
    LCD_WR_Bus(dat & 0xFF);
    LCD_DC_Set();
}
//static uint8_t buffDMA[512];
void LCD_WR_Busbuf(uint8_t* dat, uint32_t len)
{
	LCD_DC_Set();

	// Ensure all CPU writes are flushed to memory before DMA starts
	__DSB();  // Data Synchronization Barrier

    HAL_SPI_Transmit_DMA(&hspi1, (uint8_t *)dat, len);

	// Step 4: Wait for DMA transmission to complete
	while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY);
}

