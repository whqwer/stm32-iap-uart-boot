#ifndef INC_PROTOCOL_H_
#define INC_PROTOCOL_H_
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// ==================== IAP命令定义 ====================
#define CMD_IAP_START      0x01  // 开始固件更新（带固件大小）
#define CMD_IAP_DATA       0x02  // 固件数据包（带包序号+数据）
#define CMD_IAP_END        0x03  // 结束固件更新（带CRC32）

// ==================== 响应码 ====================
#define RESP_OK            0x00  // 成功
#define RESP_ERROR         0x01  // 失败

// ==================== CRC32函数 ====================
uint32_t crc32_c(const uint8_t *data, uint32_t len);

// ==================== 协议接收函数 ====================
// 参数：buf - UART接收到的原始数据，len - 数据长度
// 返回：0成功，-1失败
int32_t Protocol_Receive(uint8_t *buf, uint32_t len);

// ==================== IAP更新接口 ====================
// 初始化IAP（在开始更新前调用一次）
void Protocol_IAP_Init(void);

// 开始IAP更新（阻塞等待固件传输完成）
// 参数：timeout_ms - 超时时间（毫秒），0表示不超时
// 返回：0成功，-1失败，-2超时
int8_t Protocol_IAP_Update(uint32_t timeout_ms);

// 获取更新进度（已接收字节数）
uint32_t Protocol_IAP_GetProgress(void);

#endif /* INC_PROTOCOL_H_ */


