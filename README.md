# STM32H5 IAP Bootloader 项目

## 📖 项目简介

这是一个运行在 **STM32H503CBTx** 微控制器上的 **串口固件更新引导程序（IAP Bootloader）**，采用**双镜像（A/B）红蓝备份机制**，确保固件升级失败时自动回滚到旧版本。

### 核心功能

通过串口远程更新设备固件，无需使用编程器（ST-Link），并具备自动故障恢复能力。

**传统固件更新方式：**
1. 拆开设备外壳
2. 连接 ST-Link 或 J-Link 编程器
3. 使用 STM32CubeProgrammer 烧录
4. **风险**：更新失败设备变砖

**使用本 Bootloader：**
1. 串口线连接设备（3根线：TX、RX、GND）
2. 发送固件文件
3. 自动完成更新和重启
4. **安全**：更新失败自动回滚到旧版本

### 双镜像（A/B）工作原理

设备 Flash 分为两个独立的镜像区：
- **镜像 A**：存储一个固件版本
- **镜像 B**：存储另一个固件版本
- **配置区**：记录当前激活的镜像、CRC 校验值、启动失败计数

**升级流程：**
1. 当前运行镜像 A → 新固件写入镜像 B
2. 写入完成后切换激活镜像为 B
3. 下次启动从镜像 B 运行
4. 如果 B 启动失败（CRC 错误或连续启动失败 3 次）→ 自动回滚到镜像 A

**关键优势：**
- ✅ 升级失败不会导致设备变砖
- ✅ 自动 CRC 校验保证固件完整性
- ✅ 启动失败计数机制检测运行时崩溃
- ✅ 断电保护：更新过程中断电可自动恢复

### 工作原理

设备启动后 Bootloader 先运行，等待 30 秒：
- **如果收到固件数据** → 更新 Flash，写入新固件
- **如果没有数据** → 跳转到用户应用程序运行

---

## ⚙️ 关键参数配置

以下参数在 `IAP/inc/iap_config.h` 中定义，理解这些参数对使用本项目至关重要：

| 参数名 | 值 | 说明 | 备注 |
|--------|-----|------|------|
| `ApplicationAddress` | `0x0800C000` | 用户程序起始地址 | Bootloader占用0-48KB，用户程序从48KB开始 |
| `APP_FLASH_SIZE` | `0x14000` (80KB) | 用户程序最大空间 | 固件不能超过此大小 |
| `PAGE_SIZE` | `0x2000` (8KB) | Flash扇区大小 | STM32H503固定值 |
| `ENABLE_PUTSTR` | `1` | 串口调试输出开关 | 1=启用，0=关闭（节省Flash） |
| 更新超时 | `30000` ms | 等待固件超时时间 | 在`iap.c`中修改 |

### 内存分配图

```
Flash (128KB 总容量)
┌─────────────────────────────────────┐ 0x08000000
│  Bootloader 区域 (48KB)              │ 
│  - 引导程序代码                      │
│  - IAP 协议处理                      │
│  - Flash 操作函数                    │
├─────────────────────────────────────┤ 0x0800C000 ← ApplicationAddress
│  用户应用程序区域 (80KB)             │
│  - 你的应用程序代码                  │
│  - 可通过串口更新                    │
└─────────────────────────────────────┘ 0x08020000

SRAM (32KB)
┌─────────────────────────────────────┐ 0x20000000
│  运行时数据 (变量、堆栈等)           │
└─────────────────────────────────────┘ 0x20008000
```

---

## � 完整工作流程

### 启动流程

```
设备上电/复位
      ↓
┌──────────────────────┐
│ Bootloader 启动      │
│ - 初始化时钟(200MHz) │
│ - 初始化UART(115200) │
│ - 设置向量表         │
└──────┬───────────────┘
       ↓
┌──────────────────────┐
│ 启动DMA接收          │
│ 等待串口数据(30秒)   │
└──────┬───────────────┘
       ↓
    收到数据?
    ┌───┴───┐
   是│      │否(超时)
    ↓       ↓
┌────────┐ ┌──────────────┐
│固件更新│ │跳转用户程序  │
│流程    │ │0x0800C000    │
└────────┘ └──────────────┘
```

### 固件更新详细流程

```
1. 接收固件数据
   ├─ 通过UART DMA接收
   ├─ 自动解析协议帧
   │  ├─ 帧头/尾: 0x7E
   │  ├─ 转义处理: 0x7A
   │  └─ CRC32 校验
   └─ 提取固件数据

2. 写入Flash
   ├─ 首次接收时自动擦除Application区域
   ├─ 按半字(2字节)对齐写入
   ├─ 自动管理写入地址
   └─ 实时进度反馈(25%/50%/75%/100%)

3. 接收完成判定
   ├─ 检测总线空闲(UART Idle中断)
   ├─ 连续空闲检测(避免误判)
   └─ 完成后显示接收字节数

4. 验证与跳转
   ├─ 数据完整性由协议层CRC32保证
   ├─ 重启设备
   └─ Bootloader检测到有效应用程序后跳转
```

---

## � 通信协议说明

### 协议帧格式

```
[0x7E][len(4)][ver][recv][send][data...][crc32(4)][0x7E]
  帧头   长度   版本 目标  源   固件数据   校验码    帧尾
```

### 关键特性

| 特性 | 说明 |
|------|------|
| **帧定界符** | `0x7E` 标记帧的开始和结束 |
| **转义机制** | 数据中的 `0x7E` → `0x7A 0x55`<br>数据中的 `0x7A` → `0x7A 0xAA` |
| **CRC32校验** | 覆盖整个数据段，确保完整性 |
| **自动解析** | Bootloader自动处理帧解析和转义 |

### 用户只需关注

1. **发送原始固件数据** - 上位机工具会自动添加协议封装
2. **波特率匹配** - 确保上位机设为 **115200**
3. **30秒超时** - 设备启动后30秒内开始发送数据

详细协议规范参见：[IAP_PROTOCOL_USAGE.md](IAP_PROTOCOL_USAGE.md)

---

## 🛠️ 使用指南

### 第一步：烧录 Bootloader（仅需一次）

#### 使用 STM32CubeIDE

1. 导入项目：File → Import → Existing Projects → 选择 `Asteroid` 文件夹
2. 编译：右键项目 → Build Project (或 `Ctrl+B`)
3. 连接 ST-Link 到开发板
4. 烧录：右键项目 → Run As → STM32 C/C++ Application

#### 使用命令行

```bash
cd Debug
make clean && make all
openocd -f interface/stlink.cfg -f target/stm32h5x.cfg \
        -c "program bootloader.elf verify reset exit"
```

### 第二步：配置用户应用程序

⚠️ **用户程序必须做以下修改，否则无法运行！**

#### 1. 修改链接脚本 `.ld` 文件

```ld
MEMORY
{
  RAM    (xrw)    : ORIGIN = 0x20000000,   LENGTH = 32K
  FLASH  (rx)     : ORIGIN = 0x0800C000,   LENGTH = 80K    /* ← 必须改为 0x0800C000 */
}
```

**原因**：Bootloader占用了 0x08000000-0x0800BFFF (48KB)，应用程序从 0x0800C000 开始。

#### 2. 在 main() 函数开头添加向量表重定位

```c
int main(void)
{
    SCB->VTOR = 0x0800C000;  /* ← 必须添加这行 */
    __DSB();
    __ISB();
    
    HAL_Init();
    SystemClock_Config();
    // ... 其他代码
}
```

**原因**：中断向量表需要指向应用程序区域，否则中断无法正常工作。

#### 3. 编译生成 .bin 文件

编译后在 `Debug` 或 `Release` 目录找到 `app.bin` 文件，用于后续更新。

### 第三步：通过串口更新固件

#### 硬件连接

```
STM32H503          USB转TTL模块
---------          ------------
PA9  (TX)   <----> RX
PA10 (RX)   <----> TX
GND         <----> GND
```

#### 更新步骤

1. **重启设备** - 让 Bootloader 运行
2. **打开串口工具** - 波特率 115200
3. **观察提示** - 会显示等待固件信息
4. **发送固件** - 使用支持协议的上位机工具发送 `app.bin`
5. **等待完成** - 观察进度和完成提示
6. **再次重启** - 运行新应用程序

#### 串口输出示例

```
Waiting for firmware update (30s timeout)...
[Protocol] Frame received
IAP: Erasing flash...
IAP: Flash erased
IAP: Writing to 0x0800C000
IAP: Progress: 25%
IAP: Progress: 50%
IAP: Progress: 75%
IAP: Progress: 100%
=== Update Successful! ===
Total received: 10240 bytes
```

---

## ❗ 常见问题

### Q1: 应用程序无法跳转运行

**症状**：Bootloader 显示 "Run to app failed"

**原因与解决**：

| 检查项 | 如何检查 | 解决方法 |
|--------|---------|---------|
| 应用程序链接脚本起始地址 | 查看 `.ld` 文件中 `FLASH ORIGIN` | 必须为 `0x0800C000` |
| 向量表重定位 | 搜索应用程序代码中 `SCB->VTOR` | 在 `main()` 开头添加 `SCB->VTOR = 0x0800C000;` |
| 应用程序大小 | `ls -lh app.bin` | 不能超过 80KB (81920字节) |
| 应用程序是否存在 | 首次烧录Bootloader后正常 | 通过IAP更新一次固件即可 |

### Q2: 固件更新失败或卡住

**可能原因**：
- 串口连接不稳定（线材质量差、接触不良）
- 上位机工具不匹配协议
- 波特率设置错误

**解决方法**：
1. 更换质量好的USB转TTL模块和杜邦线
2. 确认波特率设为 **115200**
3. 使用符合本协议的上位机工具
4. 查看串口输出的具体错误信息

### Q3: 串口无输出

**检查清单**：
- [ ] 串口号选择正确（Linux: `/dev/ttyUSB0`，Windows: `COM?`）
- [ ] 波特率设为 115200
- [ ] TX/RX 未接反（STM32的TX接模块RX，STM32的RX接模块TX）
- [ ] GND 已连接
- [ ] 设备已上电
- [ ] USB转TTL模块驱动已安装

### Q4: 如何验证应用程序配置正确

```bash
# 方法1: 查看链接脚本
grep "FLASH.*ORIGIN" your_app.ld
# 应显示: ORIGIN = 0x0800C000

# 方法2: 查看编译后的向量表
arm-none-eabi-objdump -s -j .isr_vector app.elf | head -20
# 第一行前4字节(栈指针)应在 0x20000000-0x20008000 范围
# 第二行是复位向量，应指向 0x0800C000 附近的地址

# 方法3: 使用ST-Link直接烧录测试
openocd -f interface/stlink.cfg -f target/stm32h5x.cfg \
        -c "program app.bin 0x0800C000 verify reset exit"
# 如果能正常运行，说明应用程序本身没问题
```

---

## 🔧 调整配置参数

### 修改超时时间

在 `IAP/src/iap.c` 中找到：

```c
if ((HAL_GetTick() - start_time) > 30000)  // 30秒超时
```

修改 `30000` 为其他值（单位：毫秒）

### 修改内存分配

如需调整 Bootloader 和 Application 大小，同步修改3个文件：

**1. Bootloader链接脚本** `STM32H503CBTX_FLASH.ld`
```ld
FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 48K
```

**2. IAP配置** `IAP/inc/iap_config.h`
```c
#define ApplicationAddress    0x0800C000    // 与Bootloader大小匹配
#define APP_FLASH_SIZE        (0x14000)     // 80KB
```

**3. 应用程序链接脚本**
```ld
FLASH (rx) : ORIGIN = 0x0800C000, LENGTH = 80K
```

### 关闭调试输出（节省Flash空间）

在 `IAP/inc/iap_config.h` 中：
```c
#define ENABLE_PUTSTR  0  // 0=关闭, 1=启用
```

---

## 📚 进一步了解

### 项目文件说明

| 文件 | 说明 |
|------|------|
| `IAP/src/protocol.c` | 协议解析、帧转义、CRC32校验 |
| `IAP/src/iap.c` | IAP主逻辑、更新流程控制 |
| `IAP/src/stmflash.c` | Flash擦除、写入底层操作 |
| `Core/Src/main.c` | Bootloader入口、系统初始化 |
| `IAP/inc/iap_config.h` | **所有关键参数配置** |

### 扩展阅读

- [IAP_PROTOCOL_USAGE.md](IAP_PROTOCOL_USAGE.md) - 通信协议详细规范
- [REFACTORING_SUMMARY.md](REFACTORING_SUMMARY.md) - 代码重构说明
- [STM32H503 数据手册](https://www.st.com/resource/en/datasheet/stm32h503cb.pdf)

### 技术要点

- **DMA接收** - 使用 `HAL_UARTEx_ReceiveToIdle_DMA` 高效接收数据
- **半字对齐** - Flash写入时自动处理字节对齐问题
- **双缓冲机制** - 接收缓冲区复用，节省SRAM
- **状态机解析** - 健壮的协议帧解析状态机

---

## 📄 许可证

本项目采用 MIT 许可证。  
STM32 HAL 驱动库遵循 ST 的 BSD-3-Clause 许可协议。

---

**最后更新**: 2026年1月27日
