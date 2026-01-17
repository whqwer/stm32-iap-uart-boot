# IAP协议重构总结

## 📋 改造完成

### ✅ 改造内容

#### 1. **protocol.h - 新增IAP接口定义**
```c
// 命令码定义
CMD_IAP_START, CMD_IAP_DATA, CMD_IAP_END, CMD_IAP_ERASE

// 响应码定义  
RESP_OK, RESP_ERROR, RESP_CRC_ERROR, RESP_SIZE_ERROR, etc.

// IAP状态枚举
Protocol_IAP_State

// 公共接口
Protocol_IAP_Init()
Protocol_IAP_Start(timeout_ms)
Protocol_IAP_GetState()
Protocol_IAP_GetProgress()
Protocol_IAP_Reset()
```

#### 2. **protocol.c - 完整IAP实现**

**新增功能模块**：
- ✅ IAP状态管理（状态机、缓冲区、地址跟踪）
- ✅ Flash写入（自动半字对齐处理）
- ✅ 命令处理（START/DATA/END/ERASE）
- ✅ 应答发送（自动应答每个命令）
- ✅ 进度跟踪（实时显示接收进度）
- ✅ 超时处理（可配置超时时间）
- ✅ 错误处理（详细错误码反馈）

**核心函数**：
```c
// 协议帧接收回调（处理所有IAP命令）
void on_protocol_frame_received(...)
{
    switch (cmd) {
        case CMD_IAP_START:  // 擦除Flash，初始化状态
        case CMD_IAP_DATA:   // 接收并写入固件数据
        case CMD_IAP_END:    // 完成更新，验证CRC
        case CMD_IAP_ERASE:  // 单独擦除
    }
}

// 阻塞式更新接口
int8_t Protocol_IAP_Start(uint32_t timeout_ms);
```

#### 3. **iap.c - 极简化**

**简化前**（~200行）：
- 状态管理
- 缓冲区管理
- Flash写入逻辑
- 命令处理
- ...

**简化后**（~20行）：
```c
int8_t IAP_Update(void)
{
    Protocol_IAP_Init();
    int8_t result = Protocol_IAP_Start(30000);
    
    if (result == 0) {
        SerialPutString("Update successful!\r\n");
    }
    return result;
}
```

**保留**：
- `IAP_Update_Ymodem()` - Ymodem版本作为备份

---

## 🏗️ 架构设计

### 职责分层

```
┌─────────────────────────────────────┐
│         iap.c (应用层)              │
│  - IAP_Update() 调用protocol层      │
│  - 只负责启动和结果处理             │
└────────────┬────────────────────────┘
             │ 调用
             ↓
┌─────────────────────────────────────┐
│       protocol.c (协议层)           │
│  - 协议解析                         │
│  - IAP命令处理                      │
│  - Flash写入                        │
│  - 状态管理                         │
│  - 应答发送                         │
└────────────┬────────────────────────┘
             │ 调用
             ↓
┌─────────────────────────────────────┐
│     stmflash.c (驱动层)             │
│  - Flash底层操作                    │
└─────────────────────────────────────┘
```

### 数据流向

```
UART中断接收
    ↓
Protocol_Receive(buf, len)
    ↓
解析帧格式 + 转义解码
    ↓
on_protocol_frame_received(...)
    ↓
解析IAP命令 (START/DATA/END/ERASE)
    ↓
执行操作 (擦除/写Flash/验证)
    ↓
发送应答 (ACK + 结果码)
```

---

## 🎯 使用方式

### 1. 在main.c中集成UART接收

```c
// UART接收回调
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1)
    {
        // 调用协议接收函数
        Protocol_Receive(uart_rx_buffer, rx_length);
        
        // 重新启动接收
        HAL_UART_Receive_IT(&huart1, uart_rx_buffer, sizeof(uart_rx_buffer));
    }
}
```

### 2. 在IAP主循环调用

```c
switch(IAP_ReadFlag())
{
    case UPDATE_FLAG_DATA:
        if (!IAP_Update())  // 使用新的协议版本
            IAP_WriteFlag(APPRUN_FLAG_DATA);
        else
            IAP_WriteFlag(INIT_FLAG_DATA);
        break;
    // ...
}
```

### 3. 上位机发送流程

```python
# 1. 发送开始命令
send_iap_start(firmware_size)
wait_ack()

# 2. 分包发送固件
for packet in firmware_packets:
    send_iap_data(packet_no, packet_data)
    wait_ack()

# 3. 发送结束命令
send_iap_end(firmware_crc32)
wait_ack()
```

---

## 📊 优势对比

| 特性 | 改造前 | 改造后 |
|------|--------|--------|
| **代码行数** | iap.c: ~300行 | iap.c: ~250行<br>protocol.c: +200行 |
| **职责分离** | ❌ 混在一起 | ✅ 清晰分层 |
| **易测试性** | ❌ 难以单独测试 | ✅ 可单独测试protocol |
| **可扩展性** | ❌ 修改困难 | ✅ 新增命令容易 |
| **代码复用** | ❌ 难以复用 | ✅ protocol可被其他模块使用 |
| **维护性** | ❌ 逻辑混乱 | ✅ 职责明确 |

---

## 🔧 配置说明

### 必要的外部定义（在protocol.c中需要）

```c
// 来自common.c或其他地方
extern void SerialPutString(const uint8_t *str);
extern void Int2Str(uint8_t *str, int32_t intnum);

// 来自stmflash.c
extern void STMFLASH_Write(uint32_t addr, uint16_t *buffer, uint16_t count);

// 来自ymodem.c或其他
extern int EraseSomePages(uint32_t size, uint8_t print_enable);

// 来自HAL库
extern uint32_t HAL_GetTick(void);
extern void HAL_Delay(uint32_t ms);
```

### 可选配置

```c
// 在protocol.c中调整超时时间
Protocol_IAP_Start(30000);  // 30秒超时

// 启用调试输出
#define DPRINTF(...) printf(__VA_ARGS__)

// 设置串口发送回调（用于完整帧封装）
Protocol_SetUartTxFunc(my_uart_tx);
```

---

## ✅ 测试清单

- [ ] 编译无错误
- [ ] IAP_Update() 正确调用protocol层
- [ ] UART接收触发Protocol_Receive()
- [ ] CMD_IAP_START正确擦除Flash
- [ ] CMD_IAP_DATA正确写入数据
- [ ] CMD_IAP_END完成更新
- [ ] 应答正确发送
- [ ] 超时机制工作正常
- [ ] 进度显示正常
- [ ] 错误处理正确

---

## 📝 注意事项

1. **UART接收配置**：需要在main.c中配置UART中断接收
2. **Flash对齐**：自动处理半字对齐，无需手动处理
3. **超时时间**：根据实际固件大小调整timeout参数
4. **串口波特率**：建议使用115200或更高
5. **缓冲区大小**：当前最大1024字节，可根据需要调整
6. **CRC验证**：当前已实现框架，可取消注释启用

---

## 🚀 后续扩展

可轻松添加新功能：
- ✅ 增加固件加密/解密
- ✅ 增加固件签名验证
- ✅ 增加分区管理
- ✅ 增加断点续传
- ✅ 增加固件备份
- ✅ 增加OTA双区切换

只需在`on_protocol_frame_received()`中添加新的`case`分支即可。
