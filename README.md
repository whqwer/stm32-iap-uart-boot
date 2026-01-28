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

### 内存分配图（双镜像架构）

```
Flash (128KB 总容量)
┌─────────────────────────────────────┐ 0x08000000
│  Bootloader 区域 (32KB)              │ 
│  ├─ 代码区 (24KB)                    │
│  │  - 引导程序代码                   │
│  │  - IAP 协议处理                   │
│  │  - Flash 操作函数                 │
│  │  - 双镜像管理逻辑                 │
├─────────────────────────────────────┤ 0x08006000
│  └─ 配置区 (8KB)                     │
│     - ImageConfig_t 结构体           │
│     - 激活镜像标记 (active_image)    │
│     - CRC 校验值 (crc_A, crc_B)      │
│     - 固件大小 (size_A, size_B)      │
│     - 启动计数 (boot_count)          │
│     - 更新标志 (updating)            │
├─────────────────────────────────────┤ 0x08008000 ← IMAGE_A_BASE
│  镜像 A 区域 (48KB)                  │
│  - 应用程序版本 1                    │
│  - 可通过串口更新                    │
├─────────────────────────────────────┤ 0x08014000 ← IMAGE_B_BASE
│  镜像 B 区域 (48KB)                  │
│  - 应用程序版本 2                    │
│  - 备份/新版本固件                   │
└─────────────────────────────────────┘ 0x08020000

SRAM (32KB)
┌─────────────────────────────────────┐ 0x20000000
│  运行时数据 (变量、堆栈等)           │
└─────────────────────────────────────┘ 0x20008000
```

### 配置结构体（ImageConfig_t）

```c
typedef struct {
    uint32_t magic;           // 必须为 0x41535444 ("ASTD")
    uint8_t  active_image;    // 0=镜像A, 1=镜像B
    uint8_t  updating;        // 0=空闲, 1=更新中（断电保护）
    uint8_t  boot_count;      // 启动失败计数（应用确认后清零）
    uint8_t  reserved;        // 对齐填充
    uint32_t crc_A;           // 镜像A的CRC32值
    uint32_t crc_B;           // 镜像B的CRC32值
    uint32_t size_A;          // 镜像A固件大小（0=空）
    uint32_t size_B;          // 镜像B固件大小（0=空）
} ImageConfig_t;  // 总共32字节
```

---

## 🔄 完整工作流程

### 启动流程（双镜像选择）

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
│ 双镜像启动选择逻辑              │
│ Select_Boot_Image(&config)      │
├─────────────────────────────────┤
│ 1. 检查更新标志(updating)       │
│    如果=1 → 上次更新被中断      │
│    → 清除失败镜像，回滚         │
├─────────────────────────────────┤
│ 2. 检查启动计数(boot_count)     │
│    如果≥3 → 当前镜像反复失败    │
│    → 切换到备份镜像             │
├─────────────────────────────────┤
│ 3. 验证激活镜像CRC              │
│    CRC有效 → 从该镜像启动       │
│    CRC无效 → 尝试备份镜像       │
├─────────────────────────────────┤
│ 4. 验证备份镜像CRC              │
│    CRC有效 → 从备份镜像启动     │
│    CRC无效 → 两个镜像都坏了     │
│    → 进入更新模式等待固件       │
└──────┬──────────────────────────┘
       ↓
    有有效镜像?
    ┌───┴───┐
   是│      │否
    ↓       ↓
┌────────┐ ┌──────────────┐
│跳转运行│ │等待串口固件  │
│镜像A/B │ │(30秒超时)    │
└────────┘ └──────┬───────┘
                  ↓
              收到固件数据?
              ┌───┴───┐
             是│      │否
              ↓       ↓
          ┌────────┐ ┌────────┐
          │固件更新│ │超时后  │
          │流程    │ │尝试启动│
          └────────┘ └────────┘
```

### 固件更新详细流程（双镜像写入）

```
1. 选择更新目标
   ├─ Select_Update_Target(&config)
   ├─ 读取 active_image (当前激活的镜像)
   ├─ 目标 = 1 - active_image (选择未激活的镜像)
   ├─ 例如：当前运行A(0) → 更新写入B(1)
   └─ 发送目标地址给上位机
      ├─ target=0 → 发送 "0x08008000" (镜像A地址)
      └─ target=1 → 发送 "0x08014000" (镜像B地址)

2. 标记更新开始
   ├─ Update_Start(&config, target)
   ├─ 设置 updating = 1 (断电保护标志)
   └─ 写入配置区 Flash

3. 擦除目标镜像区
   ├─ Erase_Image(target)
   ├─ 自动处理 Bank1/Bank2 跨区擦除
   └─ 48KB (6个扇区)

4. 接收并写入固件数据
   ├─ 通过UART DMA接收
   ├─ 自动解析协议帧
   │  ├─ 帧头/尾: 0x7E
   │  ├─ 转义处理: 0x7A
   │  └─ CRC32 校验
   ├─ 写入目标镜像区 Flash
   └─ 实时进度反馈

5. 更新完成处理
   ├─ Calculate_Image_CRC(target_addr, size)
   ├─ Update_Complete(&config, target, size, crc)
   │  ├─ 保存新固件的 size 和 crc
   │  ├─ 切换 active_image = target
   │  ├─ 清除 updating = 0
   │  └─ 重置 boot_count = 0
   └─ 写入配置区

6. 重启验证
   ├─ 设备重启
   ├─ Bootloader 读取配置区
   ├─ 发现 active_image = 新镜像
   ├─ 验证新镜像 CRC
   │  ├─ CRC正确 → 从新镜像启动 ✓
   │  └─ CRC错误 → 自动回滚到旧镜像 ✗
   └─ boot_count++ (启动失败计数)

7. 应用确认成功启动
   ├─ 应用代码调用 IAP_ConfirmBoot()
   ├─ 清零 boot_count = 0
   └─ 表示新固件运行正常
```

### 故障自动恢复机制

```
场景1: 更新过程中断电
┌────────────────────────────────┐
│ 写入一半时断电                 │
│ updating = 1 标志已写入        │
└────────┬───────────────────────┘
         ↓
    重新上电后
         ↓
┌────────────────────────────────┐
│ Select_Boot_Image 检测到       │
│ updating = 1                   │
├────────────────────────────────┤
│ → 清除失败镜像的 size/crc      │
│ → 重置 updating = 0            │
│ → 从旧镜像启动                 │
└────────────────────────────────┘
   结果：设备仍可正常运行 ✓

场景2: 新固件CRC校验失败
┌────────────────────────────────┐
│ 固件传输过程数据损坏           │
│ 或写入Flash时出错              │
└────────┬───────────────────────┘
         ↓
    重启后CRC验证
         ↓
┌────────────────────────────────┐
│ Verify_Image(new) = -1 失败    │
├────────────────────────────────┤
│ → 尝试验证旧镜像               │
│ → Verify_Image(old) = 0 成功   │
│ → 从旧镜像启动                 │
└────────────────────────────────┘
   结果：自动回滚到旧版本 ✓

场景3: 新固件运行崩溃
┌────────────────────────────────┐
│ 新固件有BUG，启动后死机        │
│ 设备反复重启                   │
└────────┬───────────────────────┘
         ↓
    连续3次启动失败
         ↓
┌────────────────────────────────┐
│ boot_count 每次启动 +1         │
│ boot_count >= 3 触发回滚       │
├────────────────────────────────┤
│ → 切换 active_image            │
│ → 从旧镜像启动                 │
└────────────────────────────────┘
   结果：自动回滚到稳定版本 ✓
```
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

### 第三步：通过串口更新固件（双镜像流程）

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
2. **观察串口输出** - 会显示当前启动的镜像地址
   ```
   Boot from: 0x08008000    // 或 0x08014000
   ```
3. **触发更新模式** - 发送更新命令或在30秒超时前发送数据
4. **接收目标地址** - Bootloader 会发送升级目标地址
   ```
   0x08014000              // 示例：当前运行A，升级到B
   ```
5. **上位机选择固件** - 根据接收到的地址
   - 收到 `0x08008000` → 发送 `app_a.bin`
   - 收到 `0x08014000` → 发送 `app_b.bin`
6. **发送固件** - 使用支持协议的上位机工具
7. **等待完成** - 观察进度和完成提示
8. **自动重启** - 固件更新完成后自动重启
9. **验证启动** - 观察是否从新镜像成功启动

#### 串口输出示例（完整升级流程）

```
=== STM32H503 Dual-Image IAP Bootloader ===
Reading config...
Active image: 0 (Image A)
Boot count: 0
Verifying Image A...
Image A CRC: OK
Boot from: 0x08008000

[User triggers update mode]

Entering update mode...
Update target: Image B
Target address: 0x08014000

[Host PC sends app_b.bin]

Erasing Image B...
Erase complete
Protocol frame received
Writing to 0x08014000...
Progress: 25%
Progress: 50%
Progress: 75%
Progress: 100%

=== Update Complete ===
Total received: 12288 bytes
Calculated CRC: 0xABCD1234
Switching to Image B
Resetting...

[Device reboots]

=== STM32H503 Dual-Image IAP Bootloader ===
Reading config...
Active image: 1 (Image B)
Boot count: 1
Verifying Image B...
Image B CRC: OK
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
Detected interrupted update (updating=1)
Clearing failed image...
Falling back to Image A
Boot from: 0x08008000    ✓ 回滚成功
```

**测试2：CRC校验失败**
```
=== Update Complete ===
[内部CRC计算]
CRC mismatch! Expected vs Actual

[设备重启]
Verifying Image B... FAILED
Falling back to Image A
Boot from: 0x08008000    ✓ 回滚成功
```

**测试3：新固件崩溃**
```
[新固件运行，但有BUG导致死机]
[设备复位] boot_count=1
[再次死机] boot_count=2
[第三次]   boot_count=3 → 触发回滚
Switching to Image A
Boot from: 0x08008000    ✓ 回滚成功
```

---

## ❗ 常见问题

### Q1: 应用程序无法跳转运行

**症状**：Bootloader 显示 CRC 验证失败或无有效镜像

**原因与解决**：

| 检查项 | 如何检查 | 解决方法 |
|--------|---------|---------|
| 应用程序链接地址 | 查看 `.ld` 文件中 `FLASH ORIGIN` | 必须为 `0x08008000` (镜像A) 或 `0x08014000` (镜像B) |
| 向量表重定位 | 搜索应用程序代码中 `SCB->VTOR` | 在 `main()` 开头添加，地址与链接地址一致 |
| 应用程序大小 | `ls -lh app.bin` | 不能超过 48KB (49152字节) |
| 固件地址匹配 | 确认上位机发送的固件对应目标地址 | 镜像A用 app_a.bin，镜像B用 app_b.bin |
| 配置区初始化 | 首次烧录 Bootloader 后正常 | 通过 IAP 更新一次固件即可初始化 |

### Q2: 固件更新后设备反复重启

**原因**：新固件有BUG导致启动失败，触发自动回滚机制

**现象**：
- `boot_count` 不断增加
- 达到 3 次后自动切换到旧镜像
- 串口输出显示 "boot_count >= 3, switching image"

**解决方法**：
1. 检查新固件代码是否有致命错误（如栈溢出、未初始化的外设等）
2. 确认向量表地址设置正确
3. 验证固件链接地址与写入地址匹配
4. 在应用稳定运行后调用确认机制清零 `boot_count`

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

### Q4: 如何手动切换镜像

**方法1：通过串口命令（需要实现）**

可在 Bootloader 中添加命令解析，例如：
- 发送 `"SWITCH_A"` → 强制切换到镜像A
- 发送 `"SWITCH_B"` → 强制切换到镜像B

**方法2：通过 ST-Link 修改配置区**

使用 STM32CubeProgrammer：
1. 连接 ST-Link
2. 读取地址 `0x08006000` 的配置区
3. 修改 `active_image` 字段（偏移 +4 字节）
   - 设为 `0x00` → 镜像A
   - 设为 `0x01` → 镜像B
4. 写回配置区
5. 重启设备

**方法3：擦除配置区重置**

擦除 `0x08006000` 扇区，Bootloader 会重新初始化配置，默认激活镜像A。

### Q5: 双镜像占用空间太大怎么办

**当前配置**：
- Bootloader: 32KB
- 镜像 A: 48KB
- 镜像 B: 48KB
- 总计: 128KB (正好用满)

**优化方案**：

**选项1**：减小单个镜像大小
```c
// iap_config.h
#define IMAGE_A_SIZE  (40 * 1024)  // 改为 40KB
#define IMAGE_B_SIZE  (40 * 1024)
```
优点：节省 16KB 空间  
缺点：固件大小限制更严格

**选项2**：采用单镜像 + 临时区方案（不推荐）
- 固件区: 80KB
- 临时区: 40KB（仅存储新固件，验证后拷贝到固件区）

优点：固件可用 80KB  
缺点：失去实时双备份能力，拷贝耗时

### Q6: 如何验证应用程序配置正确

```bash
# 查看链接脚本
grep "FLASH.*ORIGIN" your_app.ld
# 应显示: ORIGIN = 0x08008000 或 0x08014000

# 查看编译后的向量表
arm-none-eabi-objdump -s -j .isr_vector app.elf | head -20
# 第一行前4字节(栈指针)应在 0x20000000-0x20008000 范围
# 第二行是复位向量，应指向对应镜像区地址

# 使用 ST-Link 直接烧录测试
# 测试镜像A
openocd -f interface/stlink.cfg -f target/stm32h5x.cfg \
        -c "program app_a.bin 0x08008000 verify reset exit"

# 测试镜像B
openocd -f interface/stlink.cfg -f target/stm32h5x.cfg \
        -c "program app_b.bin 0x08014000 verify reset exit"
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

### 修改内存分配（双镜像架构）

如需调整 Bootloader 和镜像大小，需同步修改多个文件：

**1. Bootloader 链接脚本** `STM32H503CBTX_FLASH.ld`
```ld
FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 24K  // 代码区
// 配置区固定在 0x08006000, 8KB
```

**2. IAP 配置** `IAP/inc/iap_config.h`
```c
#define BOOTLOADER_SIZE   (32 * 1024)   // Bootloader总大小
#define CONFIG_BASE       0x08006000     // 配置区地址
#define IMAGE_A_BASE      0x08008000     // 镜像A起始
#define IMAGE_A_SIZE      (48 * 1024)    // 镜像A大小
#define IMAGE_B_BASE      0x08014000     // 镜像B起始
#define IMAGE_B_SIZE      (48 * 1024)    // 镜像B大小
```

**3. 应用程序链接脚本**（两个版本）
```ld
// app_a.ld
FLASH (rx) : ORIGIN = 0x08008000, LENGTH = 48K

// app_b.ld
FLASH (rx) : ORIGIN = 0x08014000, LENGTH = 48K
```

**约束条件**：
- `IMAGE_A_BASE` 必须是 `BOOTLOADER_BASE + BOOTLOADER_SIZE`
- `IMAGE_B_BASE` 必须是 `IMAGE_A_BASE + IMAGE_A_SIZE`
- 总大小不能超过 128KB
- 所有地址必须 8KB 对齐（Flash 扇区大小）

### 关闭调试输出（节省Flash空间）

在 `IAP/inc/iap_config.h` 中：
```c
#define ENABLE_PUTSTR  0  // 0=关闭, 1=启用
```

节省约 2-3KB 空间。

### 禁用双镜像功能（退回单镜像模式）

**不推荐**，但如需回退到简单单镜像模式：

1. 修改 `iap_config.h`：
```c
#define ApplicationAddress  IMAGE_A_BASE  // 固定使用镜像A
#define FLASH_IMAGE_SIZE    IMAGE_A_SIZE
```

2. 修改 `iap.c` 中的 `IAP_RunApp()`：
```c
// 替换双镜像选择逻辑为简单跳转
uint32_t boot_address = IMAGE_A_BASE;
```

3. 修改 `IAP_Update()` 中的目标地址：
```c
g_update_target_addr = IMAGE_A_BASE;  // 固定写入A区
```

但这样会失去自动回滚能力。

---

## 📚 进一步了解

### 项目文件说明

| 文件 | 说明 |
|------|------|
| `IAP/src/protocol.c` | 协议解析、帧转义、CRC32校验 |
| `IAP/src/iap.c` | IAP主逻辑、更新流程控制、跳转管理 |
| `IAP/src/iap_image.c` | **双镜像管理核心模块**<br>- 镜像选择逻辑<br>- CRC校验<br>- 配置区读写<br>- 故障检测与回滚 |
| `IAP/src/stmflash.c` | Flash擦除、写入底层操作 |
| `Core/Src/main.c` | Bootloader入口、系统初始化 |
| `IAP/inc/iap_config.h` | **所有关键参数配置**<br>- 内存布局定义<br>- ImageConfig_t结构体 |
| `IAP/inc/iap_image.h` | 双镜像管理函数声明 |

### 核心函数解析

#### 双镜像管理函数（iap_image.c）

| 函数 | 功能 |
|------|------|
| `Config_Read()` | 从 Flash 配置区读取 ImageConfig_t |
| `Config_Write()` | 写入配置区（含擦除） |
| `Config_Init()` | 初始化配置为默认值 |
| `Calculate_Image_CRC()` | 计算指定镜像的 CRC32 |
| `Verify_Image()` | 验证镜像有效性（CRC + 栈指针检查） |
| `Select_Boot_Image()` | **启动时选择有效镜像**<br>1. 检查更新标志<br>2. 检查启动计数<br>3. 验证CRC<br>4. 返回启动地址 |
| `Select_Update_Target()` | 选择升级目标（未激活镜像） |
| `Get_Update_Address()` | 获取升级目标 Flash 地址 |
| `Update_Start()` | 标记更新开始（设置 updating=1） |
| `Update_Complete()` | 更新完成（保存CRC、切换镜像） |
| `Update_Failed()` | 更新失败（清除 updating 标志） |
| `Confirm_Boot_Success()` | 确认启动成功（清零 boot_count） |
| `Erase_Image()` | 擦除指定镜像区（自动处理 Bank1/2） |

### 扩展阅读

- [IAP_PROTOCOL_USAGE.md](IAP_PROTOCOL_USAGE.md) - 通信协议详细规范
- [REFACTORING_SUMMARY.md](REFACTORING_SUMMARY.md) - 代码重构说明
- [STM32H503 数据手册](https://www.st.com/resource/en/datasheet/stm32h503cb.pdf)

### 技术要点

- **双镜像架构** - 红蓝备份机制，确保升级失败自动回滚
- **CRC32 校验** - 每个镜像独立 CRC，启动前必须验证
- **启动失败计数** - 检测运行时崩溃，连续失败 3 次自动切换镜像
- **断电保护** - `updating` 标志确保更新中断电可恢复
- **配置区持久化** - 8KB 独立扇区存储镜像管理信息
- **DMA 接收** - 使用 `HAL_UARTEx_ReceiveToIdle_DMA` 高效接收数据
- **半字对齐** - Flash 写入时自动处理字节对齐问题（STM32H5 需 16 字节对齐）
- **双缓冲机制** - 接收缓冲区复用，节省 SRAM
- **状态机解析** - 健壮的协议帧解析状态机
- **跨 Bank 擦除** - 自动处理 Flash Bank1/Bank2 边界

### 双镜像方案的优势

| 传统单镜像 | 双镜像（本项目） |
|-----------|---------------|
| 更新失败 → 设备变砖 | ✅ 自动回滚到旧版本 |
| 无法检测运行时崩溃 | ✅ 启动失败计数自动切换 |
| 断电后可能无法恢复 | ✅ updating 标志保护 |
| 需要手动恢复 | ✅ 完全自动化故障恢复 |
| 固件可用 80KB | ⚠️ 单个固件限 48KB（双份占用） |

**适用场景**：
- ✅ 远程设备，无法物理接触
- ✅ 高可靠性要求（如工业、医疗设备）
- ✅ 固件迭代频繁，需要快速回滚能力
- ✅ 网络环境不稳定，传输可能中断
- ⚠️ 固件体积较小（<48KB）

---

## 📄 许可证

本项目采用 MIT 许可证。  
STM32 HAL 驱动库遵循 ST 的 BSD-3-Clause 许可协议。

---

**最后更新**: 2026年1月28日  
**版本**: v2.0 - 双镜像（A/B）架构
