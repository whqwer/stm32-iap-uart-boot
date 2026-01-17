# IAP 协议固件更新使用说明

## 协议帧格式

```
[0x7E] [len(4, LSB)] [version] [receiver] [sender] [data...] [crc(4, LSB)] [0x7E]
```

### 字段说明：
- **0x7E**: 帧头/帧尾标志
- **len(4, LSB)**: body长度（version + receiver + sender + data），小端格式
- **version**: 协议版本号
- **receiver**: 接收者ID
- **sender**: 发送者ID
- **data**: 实际数据载荷
- **crc(4, LSB)**: CRC32校验值（覆盖len + body），小端格式

### 转义规则：
- `0x7E` → `0x7A 0x55`
- `0x7A` → `0x7A 0xAA`

## IAP 更新命令

### 1. 开始固件更新 (CMD_IAP_START = 0x01)

**数据格式**：
```
[CMD][total_size(4, LSB)]
```

**说明**：
- `CMD`: 0x01
- `total_size`: 固件总大小（字节数），小端格式

**响应**：
- 成功: ACK + RESP_OK (0x00)
- 失败: ACK + 错误码

**示例**：
```python
# 假设固件大小为 10240 字节
data = [0x01, 0x00, 0x28, 0x00, 0x00]  # CMD + 10240(小端)
send_protocol_frame(version=1, receiver=0x01, sender=0x02, data)
```

### 2. 发送固件数据 (CMD_IAP_DATA = 0x02)

**数据格式**：
```
[CMD][packet_no(2, LSB)][firmware_data...]
```

**说明**：
- `CMD`: 0x02
- `packet_no`: 数据包序号，从0开始
- `firmware_data`: 固件数据（建议每包256-512字节）

**响应**：
- 成功: ACK + RESP_OK
- 失败: ACK + 错误码

**示例**：
```python
# 发送第一包数据（包序号0）
packet_no = 0
firmware_chunk = firmware_data[0:256]
data = [0x02, packet_no & 0xFF, (packet_no >> 8) & 0xFF] + list(firmware_chunk)
send_protocol_frame(version=1, receiver=0x01, sender=0x02, data)
```

### 3. 结束固件更新 (CMD_IAP_END = 0x03)

**数据格式**：
```
[CMD][total_crc32(4, LSB)]
```

**说明**：
- `CMD`: 0x03
- `total_crc32`: 整个固件的CRC32校验值，小端格式

**响应**：
- 成功: ACK + RESP_OK
- 失败: ACK + 错误码

**示例**：
```python
# 计算固件CRC32
import zlib
crc32 = zlib.crc32(firmware_data) & 0xFFFFFFFF

data = [
    0x03,
    crc32 & 0xFF,
    (crc32 >> 8) & 0xFF,
    (crc32 >> 16) & 0xFF,
    (crc32 >> 24) & 0xFF
]
send_protocol_frame(version=1, receiver=0x01, sender=0x02, data)
```

### 4. 擦除Flash (CMD_IAP_ERASE = 0x04)

**数据格式**：
```
[CMD]
```

**说明**：
- `CMD`: 0x04

**响应**：
- 成功: ACK + RESP_OK
- 失败: ACK + RESP_FLASH_ERROR

## 响应码定义

| 响应码 | 值 | 说明 |
|--------|-----|------|
| RESP_OK | 0x00 | 成功 |
| RESP_ERROR | 0x01 | 一般错误 |
| RESP_CRC_ERROR | 0x02 | CRC校验错误 |
| RESP_SIZE_ERROR | 0x03 | 固件大小错误 |
| RESP_FLASH_ERROR | 0x04 | Flash操作失败 |

## 完整更新流程

```
1. 主机 → STM32: CMD_IAP_START (固件大小)
2. STM32 → 主机: ACK + RESP_OK
3. 主机 → STM32: CMD_IAP_DATA (包0)
4. STM32 → 主机: ACK + RESP_OK
5. 主机 → STM32: CMD_IAP_DATA (包1)
6. STM32 → 主机: ACK + RESP_OK
   ... (重复直到所有数据发送完成)
N. 主机 → STM32: CMD_IAP_END (CRC32)
N+1. STM32 → 主机: ACK + RESP_OK
```

## Python 示例代码

```python
import serial
import struct
import zlib

class IAP_Protocol:
    START_FLAG = 0x7E
    ESCAPE_FLAG = 0x7A
    ESCAPE_7E = 0x55
    ESCAPE_7A = 0xAA
    
    CMD_IAP_START = 0x01
    CMD_IAP_DATA = 0x02
    CMD_IAP_END = 0x03
    
    def __init__(self, port, baudrate=115200):
        self.ser = serial.Serial(port, baudrate, timeout=1)
    
    def crc32(self, data):
        return zlib.crc32(bytes(data)) & 0xFFFFFFFF
    
    def escape_encode(self, data):
        result = []
        for byte in data:
            if byte == self.START_FLAG:
                result.extend([self.ESCAPE_FLAG, self.ESCAPE_7E])
            elif byte == self.ESCAPE_FLAG:
                result.extend([self.ESCAPE_FLAG, self.ESCAPE_7A])
            else:
                result.append(byte)
        return result
    
    def build_frame(self, version, receiver, sender, data):
        # Body: version + receiver + sender + data
        body = [version, receiver, sender] + list(data)
        body_len = len(body)
        
        # 构造CRC输入: len(4) + body
        crc_input = list(struct.pack('<I', body_len)) + body
        crc = self.crc32(crc_input)
        
        # 转义body和CRC
        escaped_body = self.escape_encode(body)
        escaped_crc = self.escape_encode(list(struct.pack('<I', crc)))
        
        # 构造完整帧
        frame = [self.START_FLAG]
        frame.extend(struct.pack('<I', body_len))  # len不转义
        frame.extend(escaped_body)
        frame.extend(escaped_crc)
        frame.append(self.START_FLAG)
        
        return bytes(frame)
    
    def send_frame(self, version, receiver, sender, data):
        frame = self.build_frame(version, receiver, sender, data)
        self.ser.write(frame)
        # 等待响应
        response = self.ser.read(10)
        return response
    
    def update_firmware(self, firmware_file):
        # 读取固件
        with open(firmware_file, 'rb') as f:
            firmware = f.read()
        
        total_size = len(firmware)
        print(f"Firmware size: {total_size} bytes")
        
        # 1. 发送开始命令
        print("Sending IAP_START...")
        data = [self.CMD_IAP_START] + list(struct.pack('<I', total_size))
        resp = self.send_frame(1, 0x01, 0x02, data)
        if b'OK' not in resp:
            print("Failed to start IAP")
            return False
        
        # 2. 发送固件数据
        chunk_size = 256
        packet_no = 0
        for i in range(0, total_size, chunk_size):
            chunk = firmware[i:i+chunk_size]
            data = [self.CMD_IAP_DATA] + list(struct.pack('<H', packet_no)) + list(chunk)
            resp = self.send_frame(1, 0x01, 0x02, data)
            
            if b'OK' not in resp:
                print(f"Failed at packet {packet_no}")
                return False
            
            packet_no += 1
            progress = (i + len(chunk)) / total_size * 100
            print(f"Progress: {progress:.1f}%", end='\r')
        
        print()
        
        # 3. 发送结束命令
        print("Sending IAP_END...")
        crc = self.crc32(firmware)
        data = [self.CMD_IAP_END] + list(struct.pack('<I', crc))
        resp = self.send_frame(1, 0x01, 0x02, data)
        
        if b'OK' in resp:
            print("Firmware update successful!")
            return True
        else:
            print("Failed to complete IAP")
            return False

# 使用示例
if __name__ == "__main__":
    iap = IAP_Protocol('/dev/ttyUSB0', 115200)
    iap.update_firmware('firmware.bin')
```

## 注意事项

1. **Flash写入对齐**：STM32 Flash写入必须半字（2字节）对齐
2. **CRC32计算**：使用标准CRC32算法（多项式0x04C11DB7）
3. **超时处理**：建议设置30秒超时
4. **错误重试**：建议实现数据包重传机制
5. **地址配置**：确保 `ApplicationAddress` 配置正确（当前为0x0800C000）
6. **Flash大小**：当前支持最大88KB固件（`FLASH_IMAGE_SIZE`）

## 调试

如需调试，修改 `protocol.c` 中的 `DPRINTF` 宏：
```c
#define DPRINTF(...) printf(__VA_ARGS__)  // 启用调试输出
```
