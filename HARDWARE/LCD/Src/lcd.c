#include "../Inc/lcd.h"
#include "lcdfont.h"
#include <string.h>
#include "spi_func.h"
#include <stdint.h>


/**
 * @brief       Display special pattern/icon
 * @param       x: Pattern display column start coordinate
 * @param       y: Pattern display row start coordinate
 * @param       spe: Pattern data array pointer
 * @param       fc: Pattern foreground color (RGB565 format)
 * @param       bc: Pattern background color (RGB565 format)
 * @param       width: Pattern width in pixels
 * @param       height: Pattern height in pixels
 * @retval      None
 * @note        Uses DMA transfer for efficient display of custom patterns
 */
__ALIGNED(32) uint8_t lcdspe[256];  // DMA buffer must be 32-byte aligned
void LCD_ShowSpecial(uint16_t x, uint16_t y,const unsigned char spe[], uint16_t fc, uint16_t bc, uint8_t width,uint8_t height)
{
    uint8_t temp, t, m = 0;
    uint16_t i, TypefaceNum;
    TypefaceNum=(height / 8 + ((height % 8) ? 1 : 0)) * width;
    LCD_Address_Set(x, y, x + height - 1, y + width - 1);
    for (i = 0; i < TypefaceNum; i++)
    {
    	temp=spe[i];
        for (t = 0; t < 8; t++)
        {
			if (temp & (0x01 << t)){
				lcdspe[m++]=(fc>>8) & 0xFF;
				lcdspe[m++]=fc & 0xFF;
			}
			else{
				lcdspe[m++]=(bc>>8) & 0xFF;
				lcdspe[m++]=bc & 0xFF;
			}

			if (m % (2*height) == 0)
			{
				while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY)
				{
					// Optional: Add timeout detection
				}
				LCD_WR_Busbuf(lcdspe, m);
				m = 0;
				break;
			}
        }
    }
}

/**
 * @brief  Draw partial icon — leftmost clip_cols columns only, no draw-then-erase flicker
 * @param  x,y:       Top-left screen coordinate (column, row)
 * @param  spe:       Icon bitmap (row-major, LSB-first, same format as LCD_ShowSpecial)
 * @param  fc,bc:     Foreground / background colour (RGB565)
 * @param  width:     Icon height in rows  (y direction, e.g. 6)
 * @param  height:    Icon width  in cols  (x direction, e.g. 24)
 * @param  clip_cols: How many columns to actually render (0..height)
 * @note   Only writes pixels in the window [x, x+clip_cols-1] × [y, y+width-1].
 *         Previously rendered columns outside the window are untouched, so the
 *         caller must clear the right portion before calling with a smaller value.
 */
void LCD_ShowSpecialClipped(uint16_t x, uint16_t y, const unsigned char spe[],
                             uint16_t fc, uint16_t bc, uint8_t width, uint8_t height,
                             uint8_t clip_cols)
{
    if (clip_cols == 0) return;
    if (clip_cols > height) clip_cols = height;

    uint8_t temp, t, m;
    uint8_t bytes_per_row = (height / 8) + ((height % 8) ? 1 : 0); /* e.g. 3 for h=24 */

    /* Narrow the address window to clip_cols columns */
    LCD_Address_Set(x, y, x + clip_cols - 1, y + width - 1);

    for (uint8_t row = 0; row < width; row++)   /* iterate each screen row */
    {
        m = 0;
        uint8_t col = 0;
        for (uint8_t b = 0; b < bytes_per_row && col < clip_cols; b++)
        {
            temp = spe[row * bytes_per_row + b];
            for (t = 0; t < 8 && col < clip_cols; t++, col++)
            {
                if (temp & (0x01 << t)) {
                    lcdspe[m++] = (fc >> 8) & 0xFF;
                    lcdspe[m++] = fc & 0xFF;
                } else {
                    lcdspe[m++] = (bc >> 8) & 0xFF;
                    lcdspe[m++] = bc & 0xFF;
                }
            }
        }
        if (m > 0) {
            while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY);
            LCD_WR_Busbuf(lcdspe, m);
        }
    }
}
//void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t sizey)
//{
//
//    uint8_t temp, sizex, t, m = 0;
//    uint16_t i, TypefaceNum; // Bytes occupied by one character
//    sizex = sizey / 2;
//    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizex;
//    num = num - ' ';                                     // Calculate offset
//    LCD_Address_Set(x, y, x + sizey - 1, y + sizex - 1); // Set display area
//    for (i = 0; i < TypefaceNum; i++)
//    {
//        if (sizey == 16)
//            temp = ascii_1608[num][i];
////        else if (sizey == 24)
////            temp = ascii_2412[num][i];
////        else if (sizey == 32)
////            temp = ascii_3216[num][i];
//        else
//            return;
//        for (t = 0; t < 8; t++)
//        {
//			if (temp & (0x01 << t))
//				LCD_WR_DATA(fc);
//			else
//				LCD_WR_DATA(bc);
//			m++;
//			if (m % sizey == 0)
//			{
//				m = 0;
//				break;
//			}
//        }
//    }
//}
//void LCD_ShowString(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint16_t sizey)
//{
//	while ((*s <= '~') && (*s >= ' ')) // 鍒ゆ柇鏄笉鏄潪娉曞瓧绗�
//	{
//		if(x>=120)
//		{
//			LCD_Fill(0, 0, 120, 240, BLACK);
////			LCD_ShowSpecial(64, 128,spe1616, RED, BLACK, 16,16);
//			x=0;
////			LCD_ShowSpecial(64, 128,spe1616, fc, bc, 16,16);
////			while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY)
////			{
////				// 可选：添加超时检测
////			}
//		}
//		LCD_ShowCharDMA(x, y, *s, fc, bc, sizey);
//		y += sizey / 2;
//		s++;
//		if(y%LCD_W==0){
//			x+=sizey;
//			y=0;
//		}
//    }
//}
// DMA buffer must be 32-byte aligned for STM32H5 cache coherency
__ALIGNED(32) uint8_t buffShowChar[512];  // DMA transfer buffer for character display

/* DMA buffer for ascii_1206 font: 6 cols x 12 rows x 2 bytes = 144 bytes */
__ALIGNED(32) static uint8_t buff1206[160];

/**
 * @brief       Display single character from ascii_1206 font by direct array index
 * @param       x: Character display column start coordinate (row)
 * @param       y: Character display row start coordinate (column)
 * @param       idx: Direct index into ascii_1206 array (0-16)
 * @param       fc: Foreground color (RGB565)
 * @param       bc: Background color (RGB565)
 * @retval      None
 * @note        ascii_1206 font: 12px tall, 6px wide per glyph
 */
void LCD_ShowChar1206(uint16_t x, uint16_t y, uint8_t idx, uint16_t fc, uint16_t bc)
{
    const uint8_t sizey = 12;        /* glyph height in pixels */
    const uint8_t TypefaceNum = 12;  /* (12/8+1)*6 = 12 bytes per glyph */
    uint8_t  temp, m = 0;
    uint32_t cnt = 0;

    LCD_Address_Set(x, y, x + sizey - 1, y + 5);  /* 12 rows tall, 6 cols wide */
    for (uint16_t i = 0; i < TypefaceNum; i++) {
        temp = ascii_1206[idx][i];
        for (uint8_t t = 0; t < 8; t++) {
            if (temp & (0x01 << t)) {
                buff1206[cnt++] = (fc >> 8) & 0xFF;
                buff1206[cnt++] = fc & 0xFF;
            } else {
                buff1206[cnt++] = (bc >> 8) & 0xFF;
                buff1206[cnt++] = bc & 0xFF;
            }
            m++;
            if (m % sizey == 0) {
                m = 0;
                break;
            }
        }
    }
    LCD_WR_Busbuf(buff1206, cnt);
    while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY);
    memset(buff1206, 0, sizeof(buff1206));
}

/* ── ascii_1206 字符集索引映射 ────────────────────────────────────────────────
 * ascii_1206 是专用字符集，索引非 ASCII 偏移，需手动映射。
 * 字符集涵盖 "Please restart the device" / "Screen" / "Camera" 所需的全部字符。
 * 未知字符映射为空格（索引 5）。
 *
 * 索引对照：
 *   0=P  1=l  2=e  3=a  4=s  5=(space)  6=r  7=t  8=h
 *   9=d  10=v  11=i  12=c  13=S  14=n  15=C  16=m
 * ──────────────────────────────────────────────────────────────────────────── */
static uint8_t char_to_1206_idx(char c)
{
    switch (c) {
    case 'P': return  0;
    case 'l': return  1;
    case 'e': return  2;
    case 'a': return  3;
    case 's': return  4;
    case ' ': return  5;
    case 'r': return  6;
    case 't': return  7;
    case 'h': return  8;
    case 'd': return  9;
    case 'v': return 10;
    case 'i': return 11;
    case 'c': return 12;
    case 'S': return 13;
    case 'n': return 14;
    case 'C': return 15;
    case 'm': return 16;
    case 'o': return 17;
	case 'w': return 18;
	case 'b': return 19;
	case 'y': return 20;
    default:  return  5;  /* 未知字符显示为空格 */
    }
}

/* 一次性 DMA 缓冲：最多 32 字符 × 6列 × 12行 × 2字节 = 4608 字节 */
#define LCD_1206_MAX_CHARS  32
__ALIGNED(32) static uint8_t buff1206_str[LCD_1206_MAX_CHARS * 6 * 12 * 2];

/**
 * @brief  用 ascii_1206 字体一次性 DMA 渲染字符串（单次 SPI 传输，无逐字闪烁）
 * @param  x:  行起始坐标（字符高度方向，占 12 像素）
 * @param  y:  列起始坐标（字符宽度方向，每字符 6 像素）
 * @param  s:  要显示的字符串（仅支持 ascii_1206 字符集内的字符）
 * @param  fc: 前景色 RGB565
 * @param  bc: 背景色 RGB565
 * @note   字符集：P l e a s (space) r t h d v i c S n C m
 *         超出字符集的字符显示为空格；最长支持 32 个字符
 */
void LCD_ShowString1206(uint16_t x, uint16_t y, const char *s,
                        uint16_t fc, uint16_t bc)
{
    uint8_t len = 0;
    while (s[len] && len < LCD_1206_MAX_CHARS) len++;
    if (len == 0) return;

    /* 一次性设置整个字符串的显示区域：12行 × (len×6)列 */
    LCD_Address_Set(x, y, x + 11, y + (uint16_t)(len * 6U) - 1U);

    uint32_t cnt = 0;
    for (uint8_t ci = 0; ci < len; ci++) {
        uint8_t idx  = char_to_1206_idx(s[ci]);
        uint8_t m    = 0;
        for (uint8_t i = 0; i < 12; i++) {
            uint8_t temp = ascii_1206[idx][i];
            for (uint8_t t = 0; t < 8; t++) {
                if (temp & (0x01U << t)) {
                    buff1206_str[cnt++] = (fc >> 8) & 0xFF;
                    buff1206_str[cnt++] =  fc        & 0xFF;
                } else {
                    buff1206_str[cnt++] = (bc >> 8) & 0xFF;
                    buff1206_str[cnt++] =  bc        & 0xFF;
                }
                m++;
                if (m % 12U == 0U) { m = 0; break; }
            }
        }
    }
    LCD_WR_Busbuf(buff1206_str, cnt);
    while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY);
    memset(buff1206_str, 0, cnt);
}

/**
 * @brief  显示 "Screen"（label=0）或 "Camera"（label=1）标签
 * @note   内部调用 LCD_ShowString1206，单次 DMA 传输
 */
void LCD_ShowStr1206(uint16_t x, uint16_t y, uint8_t label, uint16_t fc, uint16_t bc)
{
    LCD_ShowString1206(x, y, (label == 0) ? "Screen" : "Camera", fc, bc);
}

/**
 * @brief       Display single character using DMA transfer
 * @param       x: Character display column start coordinate
 * @param       y: Character display row start coordinate
 * @param       num: ASCII code of character to display
 * @param       fc: Character foreground color (RGB565)
 * @param       bc: Character background color (RGB565)
 * @param       sizey: Character size (height in pixels)
 * @retval      None
 * @note        Currently supports 16-pixel height characters only
 */
void LCD_ShowCharDMA(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint8_t temp, sizex, t, m = 0;
    uint32_t cnt=0;
    uint16_t i, TypefaceNum;  // Number of bytes occupied by one character
    sizex = sizey / 2;  // Character width is half of height
    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizex;
    num = num - ' ';  // Calculate offset from space character
    LCD_Address_Set(x, y, x + sizey - 1, y + sizex - 1);  // Set display window
    for (i = 0; i < TypefaceNum; i++)
    {
        if (sizey == 16)
            temp = ascii_1608[num][i];
        else if (sizey == 24)
            temp = ascii_2412[num][i];
//        else if (sizey == 32)
//            temp = ascii_3216[num][i];
        else
            return;
        for (t = 0; t < 8; t++)
        {
			if (temp & (0x01 << t)){
				buffShowChar[cnt++]=(fc>>8) & 0xFF;
				buffShowChar[cnt++]=fc & 0xFF;
			}
			else{
				buffShowChar[cnt++]=(bc>>8) & 0xFF;
				buffShowChar[cnt++]=bc & 0xFF;
			}
			m++;
			if (m % sizey == 0)
			{
				m = 0;
				break;
			}
        }
    }
	LCD_WR_Busbuf(buffShowChar, cnt);
	// Critical: Wait for DMA transfer to complete
	while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY)
	{
		// Optional: Add timeout detection
	}
	memset(buffShowChar,0,512);
}


/**
 * @brief       Append character to screen buffer for batch DMA transfer
 * @param       x: Character display column start coordinate (not used in current implementation)
 * @param       y: Character display row start coordinate (not used in current implementation)
 * @param       num: ASCII code of character to display
 * @param       fc: Character foreground color (RGB565)
 * @param       bc: Character background color (RGB565)
 * @param       sizey: Character size (height in pixels)
 * @retval      None
 * @note        Data is accumulated in screen buffer, call LCD_ShowStringDMA to transfer
 */
// DMA buffer must be 32-byte aligned for STM32H5 cache coherency
__ALIGNED(32) uint8_t screen[4096];  // Screen buffer: RGB565 2bytes/pixel, max 16 chars of font16 (16*256=4096) or 7 chars of font24
uint32_t screen_cnt=0;  // Screen buffer write pointer
void LCD_ShowCharDMAStr(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t sizey)
{
    uint8_t temp, t, m = 0;
    uint8_t sizex=sizey/2;
    uint16_t i, TypefaceNum; // Bytes occupied by one character
    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizex;
    num = num - ' ';
    for (i = 0; i < TypefaceNum; i++)
    {
        if (sizey == 16)
            temp = ascii_1608[num][i];
        else if (sizey == 24)
            temp = ascii_2412[num][i];
//        else if (sizey == 32)
//            temp = ascii_3216[num][i];
        else
            return;
        for (t = 0; t < 8; t++)
        {
			if (temp & (0x01 << t)){
				screen[screen_cnt++]=(fc>>8) & 0xFF;
				screen[screen_cnt++]=fc & 0xFF;
			}
			else{
				screen[screen_cnt++]=(bc>>8) & 0xFF;
				screen[screen_cnt++]=bc & 0xFF;
			}
			m++;
			if (m % sizey == 0)
			{
				m = 0;
				break;
			}
        }
    }
}
/**
 * @brief       Display string using DMA transfer with screen buffer
 * @param       x: String display column start coordinate
 * @param       y: String display row start coordinate
 * @param       s: Pointer to string to display
 * @param       fc: Text foreground color (RGB565)
 * @param       bc: Text background color (RGB565)
 * @param       sizey: Character size (height in pixels)
 * @retval      None
 * @note        Automatically wraps to next line when reaching screen edge
 */
void LCD_ShowStringDMA(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint16_t sizey)
{
	uint8_t sizex = sizey / 2;
	/* Bug fix: record the starting position and string length BEFORE the loop.
	 * After the loop s points to '\0', so strlen(s)==0 and x/y are the
	 * position *after* the last character — both cause a wrong address window. */
	uint16_t x0 = x, y0 = y;
	uint16_t str_len = (uint16_t)strlen(s);

	screen_cnt = 0;  /* Bug fix: reset accumulator so old data doesn't bleed in */

	while ((*s <= '~') && (*s >= ' '))  // Process valid ASCII characters
	{
		if(x>=120)  // Screen width boundary check
		{
			LCD_Fill(0, 0, 120, 240, BLACK);  // Clear screen on overflow
			x=0;
		}
		LCD_ShowCharDMAStr(x, y, *s, fc, bc, sizey);
		y += sizey / 2;
		s++;
		if(y%LCD_W==0){
			x+=sizey;
			y=0;
		}
	}
	/* Bug fix: use the saved start position and pre-computed length */
	LCD_Address_Set(x0, y0, x0 + sizey - 1, y0 + sizex * str_len - 1);
	LCD_WR_Busbuf(screen, screen_cnt);
	while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY);
	screen_cnt = 0;  /* reset after transfer */
}


/**
 * @brief       Calculate power (custom implementation)
 * @param       m: Base number
 * @param       n: Exponent
 * @retval      result: m to the power of n
 */
//uint32_t mypow(uint8_t m, uint8_t n)
//{
//    uint32_t result = 1;
//    while (n--)
//    {
//        result *= m;
//    }
//    return result;
//}

/**
 * @brief       Display integer number
 * @param       x: Display column start coordinate
 * @param       y: Display row start coordinate
 * @param       num: Number to display (0~4294967295)
 * @param       len: Number of digits to display
 * @param       fc: Text foreground color
 * @param       bc: Text background color
 * @param       sizey: Text size
 * @retval      None
 */
//void LCD_ShowNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey)
//{
//    uint8_t t, temp, enshow = 0;
//    uint8_t sizex = sizey / 2;
//    for (t = 0; t < len; t++)
//    {
//        temp = (num / mypow(10, len - t - 1)) % 10;
//        if (enshow == 0 && t < (len - 1))
//        {
//            if (temp == 0)
//            {
//                LCD_ShowChar(x + t * sizex, y, ' ', fc, bc, sizey);
//                continue;
//            }
//            else
//            {
//                enshow = 1;
//            }
//        }
//        LCD_ShowChar(x + t * sizex, y, temp + '0', fc, bc, sizey);
//    }
//}

/**
 * @brief       Display floating point number
 * @param       x: Display column start coordinate
 * @param       y: Display row start coordinate
 * @param       num: Floating point number to display
 * @param       pre: Number of decimal places
 * @param       len: Total number of digits (including decimal point)
 * @param       fc: Text foreground color
 * @param       bc: Text background color
 * @param       sizey: Text size
 * @retval      None
 */
//void LCD_ShowFloatNum(uint16_t x, uint16_t y, float num, uint8_t pre, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey)
//{
//    uint32_t i, temp, num1;
//    uint8_t sizex = sizey / 2;
//    num1 = num * mypow(10, pre);
//    for (i = 0; i < len; i++)
//    {
//        temp = (num1 / mypow(10, len - i - 1)) % 10;
//        if (i == (len - pre))
//        {
//            LCD_ShowChar(x + (len - pre) * sizex, y, '.', fc, bc, sizey);
//            i++;
//            len += 1;
//        }
//        LCD_ShowChar(x + i * sizex, y, temp + '0', fc, bc, sizey);
//    }
//}

/**
 * @brief       Display 12x12 Chinese characters
 * @param       x: Display column start coordinate
 * @param       y: Display row start coordinate
 * @param       *s: String start address to display
 * @param       fc: Text foreground color
 * @param       bc: Text background color
 * @param       sizey: Text size
 * @retval      None
 */
//void LCD_ShowChinese12x12(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey)
//{
//    uint8_t i, j, m = 0;
//    uint16_t k, HZnum;    // Chinese character count
//    uint16_t TypefaceNum; // Bytes occupied by one character
//
//    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
//    HZnum = sizeof(tfont12) / sizeof(typFONT_GB12); // Count total characters
//    for (k = 0; k < HZnum; k++)
//    {
//        if ((tfont12[k].Index[0] == *(s)) && (tfont12[k].Index[1] == *(s + 1)))
//        {
//            LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
//            for (i = 0; i < TypefaceNum; i++)
//            {
//                for (j = 0; j < 8; j++)
//                {
//                        if (tfont12[k].Msk[i] & (0x01 << j))
//                        {
//                            LCD_WR_DATA(fc);
//                        }
//                        else
//                        {
//                            LCD_WR_DATA(bc);
//                        }
//                        m++;
//                        if (m % sizey == 0)
//                        {
//                            m = 0;
//                            break;
//                        }
//                }
//            }
//        }
//        continue; // Continue search after finding matching character to prevent duplicate retrieval
//    }
//}

/**
 * @brief       Display 16x16 Chinese characters
 * @param       x: Display column start coordinate
 * @param       y: Display row start coordinate
 * @param       *s: String start address to display
 * @param       fc: Text foreground color
 * @param       bc: Text background color
 * @param       sizey: Text size
 * @retval      None
 */
//void LCD_ShowChinese16x16(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey)
//{
//    uint8_t i, j, m = 0;
//    uint16_t k, HZnum;    // Chinese character count
//    uint16_t TypefaceNum; // Bytes occupied by one character
//
//    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
//    HZnum = sizeof(tfont16) / sizeof(typFONT_GB16); // Count total characters
//    for (k = 0; k < HZnum; k++)
//    {
//        if ((tfont16[k].Index[0] == *(s)) && (tfont16[k].Index[1] == *(s + 1)))
//        {
//            LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
//            for (i = 0; i < TypefaceNum; i++)
//            {
//                for (j = 0; j < 8; j++)
//                {
//                        if (tfont16[k].Msk[i] & (0x01 << j))
//                        {
//                            LCD_WR_DATA(fc);
//                        }
//                        else
//                        {
//                            LCD_WR_DATA(bc);
//                        }
//                        m++;
//                        if (m % sizey == 0)
//                        {
//                            m = 0;
//                            break;
//                        }
//
//                }
//            }
//        }
//        continue; // Continue search after finding matching character to prevent duplicate retrieval
//    }
//}

/**
 * @brief       Display 24x24 Chinese characters
 * @param       x: Display column start coordinate
 * @param       y: Display row start coordinate
 * @param       *s: String start address to display
 * @param       fc: Text foreground color
 * @param       bc: Text background color
 * @param       sizey: Text size
 * @retval      None
 */
//void LCD_ShowChinese24x24(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey)
//{
//    uint8_t i, j, m = 0;
//    uint16_t k, HZnum;    // Chinese character count
//    uint16_t TypefaceNum; // Bytes occupied by one character
//
//    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
//    HZnum = sizeof(tfont24) / sizeof(typFONT_GB24); // Count total characters
//    for (k = 0; k < HZnum; k++)
//    {
//        if ((tfont24[k].Index[0] == *(s)) && (tfont24[k].Index[1] == *(s + 1)))
//        {
//            LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
//            for (i = 0; i < TypefaceNum; i++)
//            {
//                for (j = 0; j < 8; j++)
//                {
//
//                        if (tfont24[k].Msk[i] & (0x01 << j))
//                        {
//                            LCD_WR_DATA(fc);
//                        }
//                        else
//                        {
//                            LCD_WR_DATA(bc);
//                        }
//                        m++;
//                        if (m % sizey == 0)
//                        {
//                            m = 0;
//                            break;
//                        }
//                }
//            }
//        }
//        continue; // Continue search after finding matching character to prevent duplicate retrieval
//    }
//}

/**
 * @brief       Display 32x32 Chinese characters
 * @param       x: Display column start coordinate
 * @param       y: Display row start coordinate
 * @param       *s: String start address to display
 * @param       fc: Text foreground color
 * @param       bc: Text background color
 * @param       sizey: Text size
 * @retval      None
 */
//void LCD_ShowChinese32x32(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey)
//{
//    uint8_t i, j, m = 0;
//    uint16_t k, HZnum;    // Chinese character count
//    uint16_t TypefaceNum; // Bytes occupied by one character
//
//    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
//    HZnum = sizeof(tfont32) / sizeof(typFONT_GB32); // Count total characters
//    for (k = 0; k < HZnum; k++)
//    {
//        if ((tfont32[k].Index[0] == *(s)) && (tfont32[k].Index[1] == *(s + 1)))
//        {
//            LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
//            for (i = 0; i < TypefaceNum; i++)
//            {
//                for (j = 0; j < 8; j++)
//                {
//                        if (tfont32[k].Msk[i] & (0x01 << j))
//                        {
//                            LCD_WR_DATA(fc);
//                        }
//                        else
//                        {
//                            LCD_WR_DATA(bc);
//                        }
//                        m++;
//                        if (m % sizey == 0)
//                        {
//                            m = 0;
//                            break;
//                        }
//
//                }
//            }
//        }
//        continue; // Continue search after finding matching character to prevent duplicate retrieval
//    }
//}

/**
 * @brief       Display Chinese string
 * @param       x: Display column start coordinate
 * @param       y: Display row start coordinate
 * @param       *s: String to display
 * @param       fc: Text foreground color
 * @param       bc: Text background color
 * @param       sizey: Text size
 * @retval      None
 */
//void LCD_ShowChinese(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey)
//{
//    while (*s != 0)
//    {
//        if (sizey == 12)
//            LCD_ShowChinese12x12(x, y, s, fc, bc, sizey);
//        else if (sizey == 16)
//            LCD_ShowChinese16x16(x, y, s, fc, bc, sizey);
//        else if (sizey == 24)
//            LCD_ShowChinese24x24(x, y, s, fc, bc, sizey);
//        else if (sizey == 32)
//            LCD_ShowChinese32x32(x, y, s, fc, bc, sizey);
//        else
//            return;
//        s += 2;
//        x += sizey;
//    }
//}

/**
 * @brief       Display Chinese and English mixed string
 * @param       x: Display column start coordinate
 * @param       y: Display row start coordinate
 * @param       *s: String start address to display
 * @param       fc: Text foreground color
 * @param       bc: Text background color
 * @param       sizey: Text size
 * @retval      None
 */
//void LCD_ShowStr(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey)
//{
//    uint16_t x0 = x;
//    uint8_t bHz = 0; // Character or Chinese flag
//    while (*s != 0)  // String not ended
//    {
//        if (!bHz) // English character
//        {
//            if (x > (LCD_W - sizey / 2) || y > (LCD_H - sizey))
//            {
//             return;
//            }
////            if (*s > 0x80)
////            {
////                bHz = 1; // Chinese character
////            }
////            else // Character
////            {
//                if (*s == 0x0D) // Line feed character
//                {
//                    y += sizey;
//                    x = x0;
//                    s++;
//                }
//                else
//                {
//                    LCD_ShowChar(x, y, *s, fc, bc, sizey);
//                    x += sizey / 2; // Character width is half of full-width
//                }
//                s++;
////            }
//        }
////        else // Chinese character handling (commented out)
////        {
////            if (x > (LCD_W - sizey) || y > (LCD_H - sizey))
////            {
////                return;
////            }
////            bHz = 0;
////            if (sizey == 12)
////                LCD_ShowChinese12x12(x, y, s, fc, bc, sizey);
////            else if (sizey == 16)
////                LCD_ShowChinese16x16(x, y, s, fc, bc, sizey);
////            else if (sizey == 24)
////                LCD_ShowChinese24x24(x, y, s, fc, bc, sizey);
////            else
////                LCD_ShowChinese32x32(x, y, s, fc, bc, sizey);
////            s += 2;  // Chinese character takes 2 bytes (GB2312 encoding)
////            x += sizey;  // Move to next position (full-width)
////        }
//    }
//}

/**
 * @brief       Display string centered horizontally
 * @param       x: Horizontal center position
 * @param       y: Display row start coordinate
 * @param       *s: String start address to display
 * @param       fc: Text foreground color
 * @param       bc: Text background color
 * @param       sizey: Text size
 * @retval      None
 */
//void LCD_StrCenter(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey)
//{
//    uint16_t len = strlen((const char *)s);
//    uint16_t x1 = (LCD_W - len * 8) / 2;
//    LCD_ShowStringDMA
//}

/**
 * @brief       Display image/picture
 * @param       x: Image display column start coordinate
 * @param       y: Image display row start coordinate
 * @param       width: Image width
 * @param       height: Image height
 * @param       pic: Image data array
 * @retval      None
 */
//void LCD_ShowPicture(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t pic[])
//{
//    uint8_t picH, picL;
//    uint16_t i, j;
//    uint32_t k = 0;
//    LCD_Address_Set(x, y, x + width - 1, y + height - 1);
//    for (i = 0; i < height; i++)
//    {
//        for (j = 0; j < width; j++)
//        {
//            picH = pic[k * 2];
//            picL = pic[k * 2 + 1];
//            LCD_WR_DATA(picH << 8 | picL);
//            k++;
//        }
//    }
//}
/**
 * @brief       Display image/picture on LCD
 * @param       x: Image display column start coordinate
 * @param       y: Image display row start coordinate
 * @param       width: Image width in pixels
 * @param       height: Image height in pixels
 * @param       pic: Pointer to image data array (RGB565 format)
 * @retval      None
 * @note        Image data should be in RGB565 format (16-bit color)
 */
__ALIGNED(32) uint8_t picture_buf[256];  // DMA buffer must be 32-byte aligned
//void LCD_ShowPicture(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t pic[])
//{
//	uint8_t picture_cnt=0;
//    uint16_t i, j;
//    LCD_Address_Set(x, y, x + width - 1, y + height - 1);  // Set display window
//    for (i = 0; i < height; i++)  // Row iteration
//    {
//        for (j = 0; j < width; j++)  // Column iteration
//        {
////            LCD_WR_DATA(pic[width*i+j]);  // Write pixel data
//        	picture_buf[picture_cnt++]=(pic[width*i+j]>>8) & 0xFF;
//        	picture_buf[picture_cnt++]=pic[width*i+j] & 0xFF;
//        }
//        LCD_WR_Busbuf(picture_buf, picture_cnt);
//		while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY)
//		{
//			// Optional: Add timeout detection
//		}
//		memset(picture_buf,0,256);
//		picture_cnt=0;
//    }
//}
void LCD_ShowPicture(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t pic[])
{
    uint32_t pic_index = 0;
    uint32_t total_pixels = width * height * 2;
    
    LCD_Address_Set(x, y, x + height - 1, y + width - 1);  // Set display window
    LCD_DC_Set();  // Set to data mode
    
    HAL_SPI_Transmit_DMA(&hspi1, pic, total_pixels);
    // Wait for DMA transmission to complete
    while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY);
}
/**
 * @brief       Draw rectangle border on LCD
 * @param       x1: Top-left corner X coordinate
 * @param       y1: Top-left corner Y coordinate
 * @param       x2: Bottom-right corner X coordinate
 * @param       y2: Bottom-right corner Y coordinate
 * @param       color: Border color (RGB565 format)
 * @retval      None
 * @note        Draws only the border lines, not filled rectangle
 */
void LCD_DrawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
//	LCD_Fill(x1, y1, x2, y2, color);
//	LCD_Fill(x1+2, y1+2, x2-2, y2-2, BLACK);
    // Top border (1 pixel wide)
    LCD_Fill(x1, y1, x1+2, y2, color);
    // Bottom border (1 pixel wide)
    LCD_Fill(x2-2, y1, x2, y2, color);
    // Left border (1 pixel wide)
    LCD_Fill(x1, y1, x2, y1+4, color);
    // Right border (1 pixel wide)
    LCD_Fill(x1, y2-4, x2, y2, color);

}
