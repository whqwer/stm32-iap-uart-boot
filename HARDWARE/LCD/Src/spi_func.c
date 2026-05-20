#include "spi_func.h"
#include "main.h"
#include "spi.h"

/**
 * @brief  Initialize GPIO pins for LCD control (DC, RES).
 *         SCK/MOSI are handled by SPI peripheral via MX_SPI1_Init().
 *         CS is tied low on hardware.
 */
void LCD_GPIOInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    LCD_RES_CLK_ENABLE();
    LCD_DC_CLK_ENABLE();

    GPIO_InitStructure.Pin   = LCD_DC_GPIO_PIN;
    GPIO_InitStructure.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LCD_DC_GPIO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.Pin   = LCD_RES_GPIO_PIN;
    HAL_GPIO_Init(LCD_RES_GPIO_PORT, &GPIO_InitStructure);
}

/**
 * @brief  Send one byte via SPI (blocking).
 *         DC pin must be set by caller before invoking.
 */
void LCD_WR_Bus(uint8_t dat)
{
    while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY);
    HAL_SPI_Transmit(&hspi1, &dat, 1, 10);
}

/** @brief Write register command (DC=0). */
void LCD_WR_REG(uint8_t reg)
{
    LCD_DC_Clr();
    LCD_WR_Bus(reg);
}

/** @brief Write one data byte (DC=1). */
void LCD_WR_DATA8(uint8_t dat)
{
    LCD_DC_Set();
    LCD_WR_Bus(dat);
}

/** @brief Write two data bytes (DC=1). */
void LCD_WR_DATA(uint16_t dat)
{
    LCD_DC_Set();
    LCD_WR_Bus(dat >> 8);
    LCD_WR_Bus(dat & 0xFF);
}

/**
 * @brief  Bulk pixel transfer via SPI DMA (DC=1).
 *         Blocks until DMA transfer is complete.
 * @param  dat  32-byte-aligned source buffer
 * @param  len  byte count
 */
void LCD_WR_Busbuf(uint8_t *dat, uint32_t len)
{
    LCD_DC_Set();
    __DSB();
    HAL_SPI_Transmit_DMA(&hspi1, dat, len);
    while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY);
}
