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

### 升级区/运行区工作原理

设备 Flash 分为两个区域：
- **升级区（UPDATE）**：临时存储新固件版本
- **运行区（RUNAPP）**：存储当前运行的固件版本
- **配置区**：记录升级标志、运行区 CRC 校验值、固件大小

**升级流程：**
1. 设备启动 → 检查配置区的升级标志
2. 如果需要升级 → 接收新固件并写入升级区
3. 写入完成后 → 将升级区的代码复制到运行区
4. 验证运行区的 CRC 校验值 → 确保复制成功
5. 清除升级标志 → 下次启动直接运行运行区的固件

**关键优势：**
- ✅ 升级失败不会导致设备变砖
- ✅ 自动 CRC 校验保证固件完整性
- ✅ 断电保护：更新过程中断电可自动恢复
- ✅ 简单直接的升级流程

### 工作原理

设备启动后 Bootloader 先运行，等待 30 秒：
- **如果收到固件数据** → 更新 Flash，写入新固件
- **如果没有数据** → 跳转到用户应用程序运行

---

## ⚙️ 关键参数配置

以下参数在 `IAP/inc/iap_config.h` 中定义，理解这些参数对使用本项目至关重要：

| 参数名 | 值 | 说明 | 备注 |
|--------|-----|------|------|
| `BOOTLOADER_BASE` | `0x08000000` | Bootloader起始地址 | 固定不变 |
| `BOOTLOADER_SIZE` | `32KB` | Bootloader占用空间 | 代码区24KB + 配置区8KB |
| `CONFIG_BASE` | `0x08006000` | 配置区起始地址 | 存储双镜像管理信息 |
| `IMAGE_A_BASE` | `0x08008000` | 镜像A起始地址 | 第一个应用程序区 |
| `IMAGE_A_SIZE` | `48KB` | 镜像A大小 | 单个固件最大48KB |
| `IMAGE_B_BASE` | `0x08014000` | 镜像B起始地址 | 第二个应用程序区 |
| `IMAGE_B_SIZE` | `48KB` | 镜像B大小 | 单个固件最大48KB |
| `CONFIG_MAGIC` | `0x41535444` | 配置有效标识 | ASCII "ASTD" |
| `MAX_BOOT_ATTEMPTS` | `3` | 最大启动失败次数 | 超过后自动切换镜像 |
| `PAGE_SIZE` | `0x2000` (8KB) | Flash扇区大小 | STM32H503固定值 |
| `ENABLE_PUTSTR` | `1` | 串口调试输出开关 | 1=启用，0=关闭（节省Flash） |
| 更新超时 | `30000` ms | 等待固件超时时间 | 在`iap.c`中修改 |

### 内存分配图（升级区/运行区架构）

```
Flash (128KB 总容量)
┌─────────────────────────────────────┐ 0x08000000
│  Bootloader 区域 (24KB)              │ 
│  - 引导程序代码                      │
│  - IAP 协议处理                      │
│  - Flash 操作函数                    │
│  - 升级区/运行区管理逻辑              │
├─────────────────────────────────────┤ 0x08006000 ← CONFIG_BASE
│  配置区 (8KB)                        │
│  - ImageConfig_t 结构体              │
│  - 升级标志 (update_flag)            │
│  - 运行区 CRC 校验值 (run_crc)       │
│  - 运行区固件大小 (run_size)         │
├─────────────────────────────────────┤ 0x08008000 ← UPDATE_REGION_BASE
│  升级区 (48KB)                       │
│  - 临时存储新固件                    │
│  - 升级完成后复制到运行区            │
├─────────────────────────────────────┤ 0x08014000 ← RUNAPP_REGION_BASE
│  运行区 (48KB)                       │
│  - 当前运行的应用程序                │
│  - 稳定版本的固件                    │
└─────────────────────────────────────┘ 0x08020000

SRAM (32KB)
┌─────────────────────────────────────┐ 0x20000000
│  运行时数据 (变量、堆栈等)           │
└─────────────────────────────────────┘ 0x08008000
```

### 配置结构体（ImageConfig_t）

```c
typedef struct {
    uint32_t magic;           // 必须为 0x41535444 ("ASTD")
    uint8_t  update_flag;      // 0=正常运行, 1=需要升级
    uint8_t  reserved[3];      // 对齐填充
    uint32_t run_crc;          // 运行区的CRC32值
    uint32_t run_size;         // 运行区固件大小（0=空）
} ImageConfig_t;  // 总共16字节
```

---

## 🔄 完整工作流程

### 启动流程（升级区/运行区架构）

```
设备上电/复位
      ↓
┌──────────────────────┐
│ Bootloader 启动      │
│ - 初始化时钟(200MHz) │
│ - 初始化UART(115200) │
│ - 读取配置区         │
└──────┬───────────────┘
       ↓
┌─────────────────────────────────┐
│ 升级区/运行区启动选择逻辑       │
│ Select_Boot_Image(&config)      │
├─────────────────────────────────┤
│ 1. 检查升级标志(update_flag)    │
│    如果=1 → 需要升级             │
│    → 进入更新模式等待固件        │
├─────────────────────────────────┤
│ 2. 不需要升级 → 验证运行区CRC   │
│    CRC有效 → 从运行区启动        │
│    CRC无效 → 进入更新模式等待固件│
└──────┬──────────────────────────┘
       ↓
    需要升级?
    ┌───┴───┐
   是│      │否
    ↓       ↓
┌──────────┐ ┌──────────┐
│进入更新  │ │跳转运行  │
│模式      │ │运行区固件│
└────┬─────┘ └──────────┘
     ↓
  收到固件数据?
  ┌───┴───┐
  是│      │否
   ↓       ↓
┌───────┐ ┌────────┐
│固件更新│ │超时后  │
│流程    │ │尝试启动│
└───────┘ └────────┘
```

### 固件更新详细流程（升级区/运行区架构）

```
1. 标记升级开始
   ├─ 设置 update_flag = 1 (升级标志)
   └─ 写入配置区 Flash

2. 擦除升级区
   ├─ Erase_Image(0)  // 0=升级区
   ├─ 自动处理 Bank1/Bank2 跨区擦除
   └─ 48KB (6个扇区)

3. 接收并写入固件数据
   ├─ 通过UART DMA接收
   ├─ 自动解析协议帧
   │  ├─ 帧头/尾: 0x7E
   │  ├─ 转义处理: 0x7A
   │  └─ CRC32 校验
   ├─ 写入升级区 Flash
   └─ 实时进度反馈

4. 复制升级区到运行区
   ├─ Copy_Update_To_Runapp(size)
   ├─ 自动擦除运行区
   └─ 一次性复制所有代码

5. 验证运行区CRC
   ├─ Calculate_Image_CRC(RUNAPP_REGION_BASE, size)
   ├─ 比较与升级区CRC是否一致
   │  ├─ CRC一致 → 升级成功 ✓
   │  └─ CRC不一致 → 升级失败 ✗
   └─ 失败则清除升级标志，保持旧固件

6. 更新完成处理
   ├─ 保存运行区的 size 和 crc
   ├─ 清除 update_flag = 0
   └─ 写入配置区

7. 重启验证
   ├─ 设备重启
   ├─ Bootloader 读取配置区
   ├─ 发现 update_flag = 0
   ├─ 验证运行区 CRC
   │  ├─ CRC正确 → 从运行区启动 ✓
   │  └─ CRC错误 → 进入更新模式等待固件 ✗
   └─ 直接运行运行区固件
```

### 故障自动恢复机制

```
场景1: 更新过程中断电
┌────────────────────────────────┐
│ 写入一半时断电                 │
│ update_flag = 1 标志已写入     │
└────────┬───────────────────────┘
         ↓
    重新上电后
         ↓
┌────────────────────────────────┐
│ Bootloader 检测到 update_flag=1 │
├────────────────────────────────┤
│ → 进入更新模式等待固件          │
│ → 重新接收固件数据              │
└────────────────────────────────┘
   结果：设备仍可正常运行 ✓

场景2: 新固件CRC校验失败
┌────────────────────────────────┐
│ 固件传输过程数据损坏           │
│ 或写入Flash时出错              │
└────────┬───────────────────────┘
         ↓
    复制后CRC验证
         ↓
┌────────────────────────────────┐
│ 运行区CRC与升级区不一致         │
├────────────────────────────────┤
│ → 清除 update_flag = 0         │
│ → 保持旧固件不变               │
│ → 下次启动从旧固件运行         │
└────────────────────────────────┘
   结果：自动保持旧版本 ✓

场景3: 运行区固件损坏
┌────────────────────────────────┐
│ 运行区CRC校验失败              │
└────────┬───────────────────────┘
         ↓
    启动时CRC验证
         ↓
┌────────────────────────────────┐
│ Verify_Run_Image() = -1 失败    │
├────────────────────────────────┤
│ → 进入更新模式等待固件          │
│ → 重新接收并写入新固件          │
└────────────────────────────────┘
   结果：自动进入更新模式 ✓
```

---

## 🔗 通信协议说明

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

### 第二步：配置用户应用程序（双镜像架构）

⚠️ **重要：双镜像方案需要准备两个链接地址不同的固件！**

#### 方案选择

由于镜像 A（0x08008000）和镜像 B（0x08014000）链接地址不同，有两种实现方案：

**【推荐】方案一：准备两个独立固件（最可靠）**

分别编译链接到不同地址的固件：
- `app_a.bin` - 链接地址 0x08008000
- `app_b.bin` - 链接地址 0x08014000

**工作流程：**
1. Bootloader 启动时通过串口发送目标地址
   - 发送 `"0x08008000\r\n"` → 上位机发送 `app_a.bin`
   - 发送 `"0x08014000\r\n"` → 上位机发送 `app_b.bin`
2. 上位机根据接收到的地址选择对应固件发送
3. 固件链接地址与写入地址匹配，确保正常运行

**优点**：简单可靠，无额外开销  
**缺点**：需要维护两个固件文件（但代码完全相同，只是链接地址不同）

---

**方案二：统一链接地址 + 运行前拷贝（备选）**

固件统一编译为 0x08008000，升级时写入 B 区作为临时存储，启动前拷贝到 A 区运行。

**缺点**：
- 每次升级后需拷贝 48KB，浪费时间（约1秒）
- 增加 Flash 擦写次数，影响寿命
- 需要额外实现拷贝逻辑

---

#### 配置步骤（以方案一为例）

##### 1. 创建镜像 A 固件配置

修改链接脚本 `.ld` 文件：

```ld
MEMORY
{
  RAM    (xrw)    : ORIGIN = 0x20000000,   LENGTH = 32K
  FLASH  (rx)     : ORIGIN = 0x08008000,   LENGTH = 48K    /* ← 镜像A地址 */
}
```

在 `main()` 函数开头添加向量表重定位：

```c
int main(void)
{
    SCB->VTOR = 0x08008000;  /* ← 镜像A向量表地址 */
    __DSB();
    __ISB();
    
    HAL_Init();
    SystemClock_Config();
    // ... 其他代码
}
```

编译后生成 `app_a.bin`。

##### 2. 创建镜像 B 固件配置

复制同一份代码，只修改链接地址：

```ld
MEMORY
{
  RAM    (xrw)    : ORIGIN = 0x20000000,   LENGTH = 32K
  FLASH  (rx)     : ORIGIN = 0x08014000,   LENGTH = 48K    /* ← 镜像B地址 */
}
```

修改向量表重定位：

```c
int main(void)
{
    SCB->VTOR = 0x08014000;  /* ← 镜像B向量表地址 */
    __DSB();
    __ISB();
    
    HAL_Init();
    SystemClock_Config();
    // ... 其他代码
}
```

编译后生成 `app_b.bin`。

##### 3. （可选）在应用中确认启动成功

为了让 Bootloader 知道新固件运行正常，避免误判为失败，建议在应用初始化完成后调用：

```c
// 在应用程序 main() 中，初始化完成后
void Confirm_Boot_Success_To_Bootloader(void)
{
    // 读取配置区
    volatile ImageConfig_t *config = (ImageConfig_t*)0x08006000;
    
    // 检查是否需要确认
    if (config->magic == 0x41535444 && config->boot_count > 0) {
        // TODO: 通过某种机制通知 Bootloader 清零 boot_count
        // 例如：设置特定标志，下次复位时 Bootloader 读取
        // 或者：通过共享 RAM 区域传递信息
    }
}
```

**注意**：由于应用无法直接修改配置区（需要擦除整个扇区），建议在应用稳定运行一段时间（如10秒）后，通过软复位让 Bootloader 重新启动，此时 Bootloader 会自动清零 `boot_count`。

**原因**：中断向量表需要指向应用程序区域，否则中断无法正常工作。固件大小不能超过 48KB。

### 第三步：通过串口更新固件（升级区/运行区流程）

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
2. **观察串口输出** - 会显示当前启动的运行区地址
   ```
   Boot from: 0x08014000    // 运行区地址
   ```
3. **触发更新模式** - 发送更新命令或在30秒超时前发送数据
4. **接收目标地址** - Bootloader 会发送升级目标地址（固定为升级区）
   ```
   0x08008000              // 升级区地址
   ```
5. **发送固件** - 使用支持协议的上位机工具，直接发送 `app.bin`
6. **等待完成** - 观察进度和完成提示
7. **自动复制** - 升级完成后自动从升级区复制到运行区
8. **验证CRC** - 自动验证运行区的CRC校验值
9. **自动重启** - 验证成功后自动重启
10. **验证启动** - 观察是否从运行区成功启动

#### 串口输出示例（完整升级流程）

```
=== STM32H503 Update/RUNAPP IAP Bootloader ===
Reading config...
Update flag: 0
Verifying RUNAPP region...
RUNAPP CRC: OK
Boot from: 0x08014000

[User triggers update mode]

Entering update mode...
Update target: UPDATE region
Target address: 0x08008000

[Host PC sends app.bin]

Erasing UPDATE region...
Erase complete
Protocol frame received
Writing to 0x08008000...
Progress: 25%
Progress: 50%
Progress: 75%
Progress: 100%

copy update to runapp...
=== Update Complete ===
Total received: 12288 bytes
Calculated CRC: 0xABCD1234
RUNAPP CRC verified: OK
Resetting...

[Device reboots]

=== STM32H503 Update/RUNAPP IAP Bootloader ===
Reading config...
Update flag: 0
Verifying RUNAPP region...
RUNAPP CRC: OK
Boot from: 0x08014000

[Jumping to new firmware...]
```

#### 故障场景测试

**测试1：传输中断**
```
Progress: 50%
[断开连接或断电]

[设备重启]
Reading config...
Detected update flag=1
Entering update mode...
Waiting for firmware data...
[重新发送固件]
✓ 更新成功
```

**测试2：CRC校验失败**
```
=== Update Complete ===
[内部CRC计算]
CRC mismatch! Expected vs Actual

[设备重启]
Reading config...
Update flag=0
Verifying RUNAPP region... OK
Boot from: 0x08014000    ✓ 保持旧版本成功
```

**测试3：运行区固件损坏**
```
[运行区CRC校验失败]

[设备重启]
Reading config...
Verifying RUNAPP region... FAILED
Entering update mode...
Waiting for firmware data...
[发送新固件]
✓ 更新成功
```

---

## ❗ 常见问题

### Q1: 应用程序无法跳转运行

**症状**：Bootloader 显示 CRC 验证失败或无有效镜像

**原因与解决**：

| 检查项 | 如何检查 | 解决方法 |
|--------|---------|---------|
| 应用程序链接地址 | 查看 `.ld` 文件中 `FLASH ORIGIN` | 必须为 `0x08014000` (运行区地址) |
| 向量表重定位 | 搜索应用程序代码中 `SCB->VTOR` | 在 `main()` 开头添加，地址与链接地址一致 |
| 应用程序大小 | `ls -lh app.bin` | 不能超过 48KB (49152字节) |
| 配置区初始化 | 首次烧录 Bootloader 后正常 | 通过 IAP 更新一次固件即可初始化 |

### Q2: 固件更新后设备反复重启

**原因**：新固件有BUG导致启动失败，运行区CRC校验失败

**现象**：
- Bootloader 显示 "RUNAPP CRC: FAILED"
- 进入更新模式等待新固件
- 串口输出显示 "Verifying RUNAPP region... FAILED"

**解决方法**：
1. 检查新固件代码是否有致命错误（如栈溢出、未初始化的外设等）
2. 确认向量表地址设置正确
3. 验证固件链接地址是否为 `0x08014000`
4. 重新发送固件并确保传输过程稳定

### Q3: CRC 校验总是失败

**可能原因**：
- 固件传输过程数据损坏
- 串口线质量差或干扰严重
- 波特率设置错误
- Flash 写入异常

**解决方法**：
1. 更换质量好的 USB 转 TTL 模块和杜邦线
2. 确认波特率设为 **115200**
3. 减少传输距离，远离干扰源
4. 使用支持协议的上位机工具
5. 查看串口输出的具体 CRC 值对比

### Q4: 如何重置升级标志

**方法**：通过 ST-Link 修改配置区

使用 STM32CubeProgrammer：
1. 连接 ST-Link
2. 读取地址 `0x08006000` 的配置区
3. 修改 `update_flag` 字段（偏移 +4 字节）
   - 设为 `0x00` → 正常运行模式
4. 写回配置区
5. 重启设备

**注意**：如果设备无法启动，Bootloader 会自动进入更新模式等待新固件。

### Q5: 升级区/运行区占用空间太大怎么办

**当前配置**：
- Bootloader: 24KB
- 配置区: 8KB
- 升级区: 48KB
- 运行区: 48KB
- 总计: 128KB (正好用满)

**优化方案**：

**选项**：减小升级区和运行区大小
```c
// iap_config.h
#define UPDATE_REGION_SIZE  (40 * 1024)  // 改为 40KB
#define RUNAPP_REGION_SIZE  (40 * 1024)
```
优点：节省 16KB 空间  
缺点：固件大小限制更严格

### Q6: 如何验证应用程序配置正确

```bash
# 查看链接脚本
grep "FLASH.*ORIGIN" your_app.ld
# 应显示: ORIGIN = 0x08014000

# 查看编译后的向量表
arm-none-eabi-objdump -s -j .isr_vector app.elf | head -20
# 第一行前4字节(栈指针)应在 0x20000000-0x20008000 范围
# 第二行是复位向量，应指向 0x08014000 附近

# 使用 ST-Link 直接烧录测试
openocd -f interface/stlink.cfg -f target/stm32h5x.cfg \
        -c "program app.bin 0x08014000 verify reset exit"
```

---

## 🔧 调整配置参数

### 修改超时时间

在 `IAP/src/iap.c` 中找到：

```c
if ((HAL_GetTick() - start_time) > 30000)  // 30秒超时
```

修改 `30000` 为其他值（单位：毫秒）

### 修改启动失败阈值

在 `IAP/inc/iap_config.h` 中：

```c
#define MAX_BOOT_ATTEMPTS     3    // 改为其他值，如 5
```

**注意**：设置过大会导致故障固件反复重启多次才回滚，设置过小可能误判正常启动为失败。

### 修改内存分配（升级区/运行区架构）

如需调整 Bootloader 和区域大小，需同步修改多个文件：

**1. Bootloader 链接脚本** `STM32H503CBTX_FLASH.ld`
```ld
FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 24K  // 代码区
// 配置区固定在 0x08006000, 8KB
```

**2. IAP 配置** `IAP/inc/iap_config.h`
```c
#define BOOTLOADER_SIZE   (24 * 1024)   // Bootloader大小
#define CONFIG_BASE       0x08006000     // 配置区地址
#define UPDATE_REGION_BASE  0x08008000   // 升级区起始
#define UPDATE_REGION_SIZE  (48 * 1024)  // 升级区大小
#define RUNAPP_REGION_BASE  0x08014000   // 运行区起始
#define RUNAPP_REGION_SIZE  (48 * 1024)  // 运行区大小
```

**3. 应用程序链接脚本**
```ld
// app.ld
FLASH (rx) : ORIGIN = 0x08014000, LENGTH = 48K
```

**约束条件**：
- `CONFIG_BASE` 必须是 `BOOTLOADER_BASE + BOOTLOADER_SIZE`
- `UPDATE_REGION_BASE` 必须是 `CONFIG_BASE + CONFIG_SIZE`
- `RUNAPP_REGION_BASE` 必须是 `UPDATE_REGION_BASE + UPDATE_REGION_SIZE`
- 总大小不能超过 128KB
- 所有地址必须 8KB 对齐（Flash 扇区大小）

### 关闭调试输出（节省Flash空间）

在 `IAP/inc/iap_config.h` 中：
```c
#define ENABLE_PUTSTR  0  // 0=关闭, 1=启用
```

节省约 2-3KB 空间。

### 注意事项

**重要**：本项目采用升级区/运行区架构，不再支持双镜像模式。如果需要恢复到双镜像模式，需要重新修改代码结构和配置文件。

---

## 📚 进一步了解

### 项目文件说明

| 文件 | 说明 |
|------|------|
| `IAP/src/protocol.c` | 协议解析、帧转义、CRC32校验 |
| `IAP/src/iap.c` | IAP主逻辑、更新流程控制、跳转管理 |
| `IAP/src/iap_image.c` | **升级区/运行区管理核心模块**<br>- 运行区验证逻辑<br>- CRC校验<br>- 配置区读写<br>- 故障检测与处理 |
| `IAP/src/stmflash.c` | Flash擦除、写入底层操作 |
| `Core/Src/main.c` | Bootloader入口、系统初始化 |
| `IAP/inc/iap_config.h` | **所有关键参数配置**<br>- 内存布局定义<br>- ImageConfig_t结构体 |
| `IAP/inc/iap_image.h` | 升级区/运行区管理函数声明 |

### 核心函数解析

#### 升级区/运行区管理函数（iap_image.c）

| 函数 | 功能 |
|------|------|
| `Config_Read()` | 从 Flash 配置区读取 ImageConfig_t |
| `Config_Write()` | 写入配置区（含擦除） |
| `Config_Init()` | 初始化配置为默认值 |
| `Calculate_Image_CRC()` | 计算指定区域的 CRC32 |
| `Verify_Run_Image()` | 验证运行区有效性（CRC + 栈指针检查） |
| `Select_Boot_Image()` | **启动时选择运行区**<br>1. 检查升级标志<br>2. 验证运行区CRC<br>3. 返回启动地址 |
| `Update_Start()` | 标记更新开始（设置 update_flag=1） |
| `Update_Failed()` | 更新失败（清除 update_flag 标志） |
| `Confirm_Boot_Success()` | 确认启动成功（清除 update_flag 标志） |
| `Erase_Image()` | 擦除指定区域（自动处理 Bank1/2） |

### 扩展阅读

- [IAP_PROTOCOL_USAGE.md](IAP_PROTOCOL_USAGE.md) - 通信协议详细规范
- [REFACTORING_SUMMARY.md](REFACTORING_SUMMARY.md) - 代码重构说明
- [STM32H503 数据手册](https://www.st.com/resource/en/datasheet/stm32h503cb.pdf)

### 技术要点

- **升级区/运行区架构** - 临时存储 + 最终运行区机制，确保升级安全
- **CRC32 校验** - 运行区 CRC 验证，确保固件完整性
- **断电保护** - `update_flag` 标志确保更新中断电可恢复
- **配置区持久化** - 8KB 独立扇区存储升级管理信息
- **DMA 接收** - 使用 `HAL_UARTEx_ReceiveToIdle_DMA` 高效接收数据
- **半字对齐** - Flash 写入时自动处理字节对齐问题（STM32H5 需 16 字节对齐）
- **双缓冲机制** - 接收缓冲区复用，节省 SRAM
- **状态机解析** - 健壮的协议帧解析状态机
- **跨 Bank 擦除** - 自动处理 Flash Bank1/Bank2 边界
- **一次性复制** - 升级完成后一次性复制所有代码到运行区

### 升级区/运行区方案的优势

| 传统单镜像 | 升级区/运行区（本项目） |
|-----------|-------------------|
| 更新失败 → 设备变砖 | ✅ 自动保持旧版本 |
| 无法检测运行时崩溃 | ✅ 运行区CRC校验自动检测 |
| 断电后可能无法恢复 | ✅ update_flag 标志保护 |
| 需要手动恢复 | ✅ 完全自动化故障恢复 |
| 固件可用 80KB | ⚠️ 单个固件限 48KB |

**适用场景**：
- ✅ 远程设备，无法物理接触
- ✅ 高可靠性要求（如工业、医疗设备）
- ✅ 固件迭代频繁，需要快速更新能力
- ✅ 网络环境不稳定，传输可能中断
- ⚠️ 固件体积较小（<48KB）

---

## 📄 许可证

本项目采用 MIT 许可证。  
STM32 HAL 驱动库遵循 ST 的 BSD-3-Clause 许可协议。

---

**最后更新**: 2026年1月30日  
**版本**: v3.0 - 升级区/运行区架构
