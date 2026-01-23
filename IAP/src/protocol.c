/*
 * protocol.c - 简化版IAP协议实现
 * 
 * 核心思路：
 * 1. Protocol_Receive() 接收UART数据，解析协议帧
 * 2. 解析成功后调用 on_protocol_frame_received() 处理命令
 * 3. on_protocol_frame_received() 根据命令类型执行相应操作
 */

#include "protocol.h"
#include "main.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

// ==================== 外部函数声明 ====================
extern void Int2Str(uint8_t *str, int32_t intnum);
extern uint8_t EraseSomePages(uint32_t size, uint8_t print_enable);
extern void STMFLASH_Write(uint32_t addr, uint16_t *buffer, uint16_t count);

// 从 iap_config.h 获取
#ifndef ApplicationAddress
#define ApplicationAddress    0x0800C000
#endif
#ifndef FLASH_IMAGE_SIZE
#define FLASH_IMAGE_SIZE      (80 * 1024)
#endif

// ==================== 协议帧常量 ====================
#define START_END_FLAG    0x7E
#define ESCAPE_FLAG       0x7A
#define ESCAPE_7E         0x55
#define ESCAPE_7A         0xAA
#define MAX_FRAME_SIZE    1024*10

// ==================== 协议解析状态机 ====================
typedef enum {
    STATE_WAIT_START = 0,
    STATE_READ_LEN,
    STATE_READ_BODY,
    STATE_READ_CRC,
    STATE_WAIT_END_FLAG
} ParseState;

// 协议解析全局变量
static ParseState state = STATE_WAIT_START;
static uint32_t body_len = 0;
static uint32_t body_offset = 0;
static uint32_t recv_count = 0;
static uint32_t recv_crc = 0;
static uint8_t frame_buf[MAX_FRAME_SIZE];
uint8_t rx_buffer[MAX_FRAME_SIZE];
//static uint8_t crc_input[MAX_FRAME_SIZE];
//static uint8_t frame_buffer[MAX_FRAME_SIZE];
//static uint8_t decode_data[MAX_FRAME_SIZE];
// decode_data 和 frame_buffer 合并（不同协议阶段使用，可以复用）
#define crc_input rx_buffer
#define decode_data frame_buf                     // 复用 frame_buf（解码后存储位置）
#define frame_buffer crc_input                    // 复用 crc_input（帧缓冲临时存储）

static uint32_t frame_pos = 0;
static uint8_t in_frame = 0;
//static uint8_t crc_bytes[4];

// ==================== IAP更新状态（简化版）====================
static uint8_t iap_started = 0;           // IAP是否已开始
static uint32_t iap_write_addr = ApplicationAddress;  // 当前Flash写入地址
//static uint8_t iap_buffer[1024*5];          // 写入缓冲区（存储半字对齐前的数据）
// ⚠️ iap_buffer 复用 frame_buf（IAP 写入时不需要接收新帧）
#define iap_buffer frame_buf                      // 复用 frame_buf（5KB）
static uint16_t iap_buf_idx = 0;          // 缓冲区当前索引
static uint32_t iap_total_received = 0;   // 总接收字节数



// CRC32 查表（已预计算，支持 input/output reflection）
const uint32_t crc32_table[256] = {
		0x00000000, 0x77073096,  0xEE0E612C, 0x990951BA,   0x076DC419, 0x706AF48F,  0xE963A535, 0x9E6495A3,
		0x0EDB8832, 0x79DCB8A4,  0xE0D5E91E, 0x97D2D988,   0x09B64C2B, 0x7EB17CBD,  0xE7B82D07, 0x90BF1D91,
		0x1DB71064, 0x6AB020F2,  0xF3B97148, 0x84BE41DE,   0x1ADAD47D, 0x6DDDE4EB,  0xF4D4B551, 0x83D385C7,
		0x136C9856, 0x646BA8C0,  0xFD62F97A, 0x8A65C9EC,   0x14015C4F, 0x63066CD9,  0xFA0F3D63, 0x8D080DF5,
		0x3B6E20C8, 0x4C69105E,  0xD56041E4, 0xA2677172,   0x3C03E4D1, 0x4B04D447,  0xD20D85FD, 0xA50AB56B,
		0x35B5A8FA, 0x42B2986C,  0xDBBBC9D6, 0xACBCF940,   0x32D86CE3, 0x45DF5C75,  0xDCD60DCF, 0xABD13D59,
		0x26D930AC, 0x51DE003A,  0xC8D75180, 0xBFD06116,   0x21B4F4B5, 0x56B3C423,  0xCFBA9599, 0xB8BDA50F,
		0x2802B89E, 0x5F058808,  0xC60CD9B2, 0xB10BE924,   0x2F6F7C87, 0x58684C11,  0xC1611DAB, 0xB6662D3D,
		0x76DC4190, 0x01DB7106,  0x98D220BC, 0xEFD5102A,   0x71B18589, 0x06B6B51F,  0x9FBFE4A5, 0xE8B8D433,
		0x7807C9A2, 0x0F00F934,  0x9609A88E, 0xE10E9818,   0x7F6A0DBB, 0x086D3D2D,  0x91646C97, 0xE6635C01,
		0x6B6B51F4, 0x1C6C6162,  0x856530D8, 0xF262004E,   0x6C0695ED, 0x1B01A57B,  0x8208F4C1, 0xF50FC457,
		0x65B0D9C6, 0x12B7E950,  0x8BBEB8EA, 0xFCB9887C,   0x62DD1DDF, 0x15DA2D49,  0x8CD37CF3, 0xFBD44C65,
		0x4DB26158, 0x3AB551CE,  0xA3BC0074, 0xD4BB30E2,   0x4ADFA541, 0x3DD895D7,  0xA4D1C46D, 0xD3D6F4FB,
		0x4369E96A, 0x346ED9FC,  0xAD678846, 0xDA60B8D0,   0x44042D73, 0x33031DE5,  0xAA0A4C5F, 0xDD0D7CC9,
		0x5005713C, 0x270241AA,  0xBE0B1010, 0xC90C2086,   0x5768B525, 0x206F85B3,  0xB966D409, 0xCE61E49F,
		0x5EDEF90E, 0x29D9C998,  0xB0D09822, 0xC7D7A8B4,   0x59B33D17, 0x2EB40D81,  0xB7BD5C3B, 0xC0BA6CAD,
		0xEDB88320, 0x9ABFB3B6,  0x03B6E20C, 0x74B1D29A,   0xEAD54739, 0x9DD277AF,  0x04DB2615, 0x73DC1683,
		0xE3630B12, 0x94643B84,  0x0D6D6A3E, 0x7A6A5AA8,   0xE40ECF0B, 0x9309FF9D,  0x0A00AE27, 0x7D079EB1,
		0xF00F9344, 0x8708A3D2,  0x1E01F268, 0x6906C2FE,   0xF762575D, 0x806567CB,  0x196C3671, 0x6E6B06E7,
		0xFED41B76, 0x89D32BE0,  0x10DA7A5A, 0x67DD4ACC,   0xF9B9DF6F, 0x8EBEEFF9,  0x17B7BE43, 0x60B08ED5,
		0xD6D6A3E8, 0xA1D1937E,  0x38D8C2C4, 0x4FDFF252,   0xD1BB67F1, 0xA6BC5767,  0x3FB506DD, 0x48B2364B,
		0xD80D2BDA, 0xAF0A1B4C,  0x36034AF6, 0x41047A60,   0xDF60EFC3, 0xA867DF55,  0x316E8EEF, 0x4669BE79,
		0xCB61B38C, 0xBC66831A,  0x256FD2A0, 0x5268E236,   0xCC0C7795, 0xBB0B4703,  0x220216B9, 0x5505262F,
		0xC5BA3BBE, 0xB2BD0B28,  0x2BB45A92, 0x5CB36A04,   0xC2D7FFA7, 0xB5D0CF31,  0x2CD99E8B, 0x5BDEAE1D,
		0x9B64C2B0, 0xEC63F226,  0x756AA39C, 0x026D930A,   0x9C0906A9, 0xEB0E363F,  0x72076785, 0x05005713,
		0x95BF4A82, 0xE2B87A14,  0x7BB12BAE, 0x0CB61B38,   0x92D28E9B, 0xE5D5BE0D,  0x7CDCEFB7, 0x0BDBDF21,
		0x86D3D2D4, 0xF1D4E242,  0x68DDB3F8, 0x1FDA836E,   0x81BE16CD, 0xF6B9265B,  0x6FB077E1, 0x18B74777,
		0x88085AE6, 0xFF0F6A70,  0x66063BCA, 0x11010B5C,   0x8F659EFF, 0xF862AE69,  0x616BFFD3, 0x166CCF45,
		0xA00AE278, 0xD70DD2EE,  0x4E048354, 0x3903B3C2,   0xA7672661, 0xD06016F7,  0x4969474D, 0x3E6E77DB,
		0xAED16A4A, 0xD9D65ADC,  0x40DF0B66, 0x37D83BF0,   0xA9BCAE53, 0xDEBB9EC5,  0x47B2CF7F, 0x30B5FFE9,
		0xBDBDF21C, 0xCABAC28A,  0x53B39330, 0x24B4A3A6,   0xBAD03605, 0xCDD70693,  0x54DE5729, 0x23D967BF,
		0xB3667A2E, 0xC4614AB8,  0x5D681B02, 0x2A6F2B94,   0xB40BBE37, 0xC30C8EA1,  0x5A05DF1B, 0x2D02EF8D
};
// 计算缓冲区的CRC32值（多项式0x04C11DB7，标准CRC32）
// 参数：data-数据缓冲区，len-数据长度，init_crc-初始值（通常传0xFFFFFFFF）
// 返回：最终CRC32值（如需标准输出，需异或0xFFFFFFFF）
uint32_t crc32_calc(const uint8_t *data, uint32_t len, uint32_t init_crc) {
    uint32_t crc = init_crc;
    while (len--) {
        crc = (crc >> 8) ^ crc32_table[(crc & 0xFF) ^ *data++];
    }
    return crc;
}
// 简化接口：直接计算标准CRC32（初始值0xFFFFFFFF，结果异或0xFFFFFFFF）
uint32_t crc32_c(const uint8_t *data, uint32_t len) {
    return crc32_calc(data, len, 0xFFFFFFFF) ^ 0xFFFFFFFF;
}


// 转义解码函数
// original	escaped
// 0x7E	0x7A 0x55
// 0x7A	0x7A 0xAA
uint32_t decode_escape(uint8_t *dst, const uint8_t *src, uint32_t src_len)
{
    uint32_t dst_idx = 0;
    for (uint32_t i = 0; i < src_len; i++)
    {
    	// only treat interior bytes (not the first/last few header/footer bytes) as candidates for escape sequences
    	if( i>=5 && i < src_len-5 )
    	{
			if (src[i] == ESCAPE_FLAG)
			{
				if (src[i+1] == ESCAPE_7E)
				{
					dst[dst_idx++] = START_END_FLAG;
					i++;
				}
				else if (src[i+1] == ESCAPE_7A) {
					dst[dst_idx++] = ESCAPE_FLAG;
					i++;
				}
	//			else
	//			{
	//				dst[dst_idx++] = src[i]; // 未知转义？直接输出
	//			}
			}
			else
			{
				dst[dst_idx++] = src[i];
			}
    	}
    	else
    	{
			dst[dst_idx++] = src[i];
		}
    }
    return dst_idx; // 返回解码后长度
}
// 实现转义编码
uint32_t encode_escape(uint8_t *dst, const uint8_t *src, uint32_t src_len)
{
    uint32_t dst_idx = 0;
    for (uint32_t i = 0; i < src_len; i++) {
    	// 只有起始和结束的0x7E标志位不转义（第0字节和最后1字节）
    	if( i<5 || i>=src_len-5 )
    	{
    		dst[dst_idx++] = src[i];
    	}
    	else{
			switch (src[i]) {
				case START_END_FLAG:  // 0x7E → 0x7A 0x55
					dst[dst_idx++] = ESCAPE_FLAG;     // 0x7A
					dst[dst_idx++] = ESCAPE_7E;       // 0x55
					break;
				case ESCAPE_FLAG:     // 0x7A → 0x7A 0xAA
					dst[dst_idx++] = ESCAPE_FLAG;     // 0x7A
					dst[dst_idx++] = ESCAPE_7A;       // 0xAA
					break;
				default:
					dst[dst_idx++] = src[i];
					break;
			}
    	}
    }
    return dst_idx;
}

// ==================== IAP公共接口 ====================
extern UART_HandleTypeDef huart1;
void Protocol_IAP_Init(void)
{
    iap_started = 0;
    iap_write_addr = ApplicationAddress;
    iap_buf_idx = 0;
    iap_total_received = 0;
    
    SerialPutString("IAP Init OK\r\n");

	// 1. 终止所有正在进行的 UART 操作
	HAL_UART_AbortReceive_IT(&huart1);
	HAL_UART_Abort(&huart1);
	
	// 2. 清除所有 UART 错误标志
	__HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_PEF);   // Parity Error
	__HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_FEF);   // Frame Error
	__HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_NEF);   // Noise Error
	__HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_OREF);  // Overrun Error
	__HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_IDLEF); // Idle Line
	__HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_RTOF);  // Receiver Timeout
	
	// 3. 清空接收 FIFO（读取所有数据）
	while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE))
	{
		(void)huart1.Instance->RDR;  // 读取并丢弃
	}
	
	// 4. 重置 UART 错误代码
	huart1.ErrorCode = HAL_UART_ERROR_NONE;
	
	// 5. 等待状态完全恢复
	HAL_Delay(10);
	
	SerialPutString("UART fully reset\r\n");
}

uint32_t Protocol_IAP_GetProgress(void)
{
    return iap_total_received;
}





// 帧格式：
// [0x7E] [len(4, LSB)] [version] [receiver] [sender] [data...] [crc(4, LSB)] [0x7E]
static uint8_t crc_bytes[4];
uint8_t boot_to_FPGA_UL1[8];
void parse_byte(uint8_t byte)
{
    switch (state) {
        case STATE_WAIT_START:
            if (byte == START_END_FLAG) {
                state = STATE_READ_LEN;
                recv_count = 0;
                body_offset = 0;
                body_len = 0;
            }
            break;

        case STATE_READ_LEN:
            ((uint8_t*)&body_len)[recv_count] = byte;
            recv_count++;
            if (recv_count == 4) {
                if (body_len < 3 || body_len > MAX_FRAME_SIZE - 10) {
                    state = STATE_WAIT_START;
                    break;
                }
                state = STATE_READ_BODY;
                recv_count = 0;
            }
            break;

        case STATE_READ_BODY:
			if (body_offset < body_len && body_offset < MAX_FRAME_SIZE) {
				frame_buf[body_offset++] = byte;
			}
			if (body_offset == body_len) {
				state = STATE_READ_CRC;
				recv_count = 0;
				memset(crc_bytes, 0, 4);  // 清零 CRC 缓冲区
			}
            break;

        case STATE_READ_CRC:

            crc_bytes[recv_count] = byte;
            recv_count++;
            if (recv_count == 4) {
                // 显式小端组合
                recv_crc = (uint32_t)crc_bytes[0] |
                           ((uint32_t)crc_bytes[1] << 8) |
                           ((uint32_t)crc_bytes[2] << 16) |
                           ((uint32_t)crc_bytes[3] << 24);
                state = STATE_WAIT_END_FLAG;
            }
            break;
        case STATE_WAIT_END_FLAG:
            if (byte == START_END_FLAG) {
				// 构造 CRC 输入：[body_length(4)] + [frame_buf(body_len)]
				crc_input[0] = (uint8_t)(body_len >> 0);
				crc_input[1] = (uint8_t)(body_len >> 8);
				crc_input[2] = (uint8_t)(body_len >> 16);
				crc_input[3] = (uint8_t)(body_len >> 24);
				memcpy(&crc_input[4], frame_buf, body_len);

				// CRC 校验
				uint32_t calc_crc = crc32_c(crc_input, 4 + body_len);
				
				if (calc_crc == recv_crc) {
					// CRC OK! 提取固件数据（跳过version, receiver, sender这3个字节）
//					uint8_t receive=frame_buf[1];
					uint8_t *firmware_data = &frame_buf[7];
					uint32_t data_len = body_len - 7;
					memcpy(boot_to_FPGA_UL1,&frame_buf[3],4);
					if ( data_len > 0)
					{
						// 将数据拷贝到缓冲区
						if (iap_buf_idx + data_len <= sizeof(iap_buffer))
						{
							memcpy(&iap_buffer[iap_buf_idx], firmware_data, data_len);
							iap_buf_idx += data_len;
							
							// 当缓冲区有足够数据时写入Flash（必须半字对齐）
							if (iap_buf_idx >= 2)
							{
								uint16_t write_count = iap_buf_idx / 2;
								STMFLASH_Write(iap_write_addr, (uint16_t*)iap_buffer, write_count);
								
								iap_write_addr += write_count * 2;
								iap_total_received += write_count * 2;
								
								// 保留未对齐的字节
								if (iap_buf_idx % 2)
								{
									iap_buffer[0] = iap_buffer[iap_buf_idx - 1];
									iap_buf_idx = 1;
								}
								else
								{
									iap_buf_idx = 0;
								}
								
//								// 打印进度
//								if ((iap_total_received % 1024) == 0)
//								{
//									SerialPutString((const uint8_t*)".");
//								}
							}
							//OK
							boot_to_FPGA_UL1[4] = (uint8_t)(0x00 >> 0);
							boot_to_FPGA_UL1[5] = (uint8_t)(0x00 >> 8);
							boot_to_FPGA_UL1[6] = (uint8_t)(0x00 >> 16);
							boot_to_FPGA_UL1[7] = (uint8_t)(0x00 >> 24);
							send_protocol_frame( 0x01, 0x00, boot_to_FPGA_UL1, 8);
						}
						else//length error
						{
							boot_to_FPGA_UL1[4] = (uint8_t)(0x01 >> 0);
							boot_to_FPGA_UL1[5] = (uint8_t)(0x01 >> 8);
							boot_to_FPGA_UL1[6] = (uint8_t)(0x01 >> 16);
							boot_to_FPGA_UL1[7] = (uint8_t)(0x01 >> 24);
							send_protocol_frame( 0x01, 0x00, boot_to_FPGA_UL1, 8);
						}
					}
				} else {//CRC error
					boot_to_FPGA_UL1[4] = (uint8_t)(0x01 >> 0);
					boot_to_FPGA_UL1[5] = (uint8_t)(0x01 >> 8);
					boot_to_FPGA_UL1[6] = (uint8_t)(0x01 >> 16);
					boot_to_FPGA_UL1[7] = (uint8_t)(0x01 >> 24);
					send_protocol_frame( 0x01, 0x00, boot_to_FPGA_UL1, 8);
				}
            }
            state = STATE_WAIT_START;
            break;
    }
}
// 协议接收函数
// 参数：buf - 接收缓冲区，len - 接收数据长度
// 返回：0 成功，-1 失败
int32_t Protocol_Receive(uint8_t *buf, uint32_t len)
{
	for (uint32_t i = 0; i < len; i++)
	{
		uint8_t byte = buf[i];
		
		if (!in_frame)
		{
			// 寻找帧头 0x7E
			if (byte == 0x7E)
			{
				in_frame = 1;
				frame_pos = 0;
				frame_buffer[frame_pos++] = byte;
				state=STATE_WAIT_START;
			}
		}
		else
		{
			// 已进入帧，继续收集
			if (frame_pos < MAX_FRAME_SIZE)
			{
				frame_buffer[frame_pos++] = byte;
				// 检查是否到达帧尾
				if (byte == 0x7E && frame_pos >= 13)
				{
					// 收到完整帧！进行转义解码
					uint32_t decoded_len = decode_escape(decode_data, frame_buffer, frame_pos);
					
					// 逐字节解析协议
					for (uint32_t j = 0; j < decoded_len; j++)
					{
						parse_byte(decode_data[j]);
					}
					
					// 重置帧状态，准备接收下一帧
					in_frame = 0;
					frame_pos = 0;
				}
			}
			else
			{
				// 缓冲区溢出，重置状态
				SerialPutString((const uint8_t*)"Frame buffer overflow!\r\n");
				in_frame = 0;
				frame_pos = 0;
				return -1;
			}
		}
	}
	return 0;
}

uint8_t body[15];      //[version] [receiver] [sender] [data...]
uint8_t crc_input_send[12]; // data_len(4) + data(8)
uint8_t raw_frame[21]; // 转义前的数据
uint8_t escaped_buf[40];// 预留足够空间用于转义后膨胀
//[0x7E] [len(4, LSB)] [version] [receiver] [sender] [data...] [crc(4, LSB)] [0x7E]
//[cmd (2,LSB)] [page_index (2,LSB)] [status (4,LSB)]
void send_protocol_frame(uint8_t receiver, uint8_t sender, const uint8_t *data, uint32_t data_len)
{
    // 帧体 = version(1) + receiver(1) + sender(1) + data(data_len)
    uint32_t body_len = 3 + data_len;

    body[0] = 0x01;           // version
    body[1] = receiver;
    body[2] = sender;
    memcpy(&body[3], data, data_len);

    // 计算 CRC 输入: [body_len(小端4字节)] + body
    crc_input_send[0] = (uint8_t)(body_len >> 0);
    crc_input_send[1] = (uint8_t)(body_len >> 8);
    crc_input_send[2] = (uint8_t)(body_len >> 16);
    crc_input_send[3] = (uint8_t)(body_len >> 24);
    memcpy(&crc_input_send[4], body, body_len);

    uint32_t calc_crc = crc32_c(crc_input_send, 4 + body_len);

    // 构造未转义的原始帧
    int pos = 0;

    raw_frame[pos++] = 0x7E;  // start flag

    // 写入 length (小端)
    raw_frame[pos++] = (uint8_t)(body_len >> 0);
    raw_frame[pos++] = (uint8_t)(body_len >> 8);
    raw_frame[pos++] = (uint8_t)(body_len >> 16);
    raw_frame[pos++] = (uint8_t)(body_len >> 24);

    // 写入 body
    memcpy(&raw_frame[pos], body, body_len);
    pos += body_len;

    // 写入 CRC (小端)
    raw_frame[pos++] = (uint8_t)(calc_crc >> 0);
    raw_frame[pos++] = (uint8_t)(calc_crc >> 8);
    raw_frame[pos++] = (uint8_t)(calc_crc >> 16);
    raw_frame[pos++] = (uint8_t)(calc_crc >> 24);

    raw_frame[pos++] = 0x7E;  // end flag

    // 转义编码
    uint32_t escaped_len = encode_escape(escaped_buf, raw_frame, pos);
    HAL_UART_Transmit(&huart1, escaped_buf, escaped_len,1000);

}
