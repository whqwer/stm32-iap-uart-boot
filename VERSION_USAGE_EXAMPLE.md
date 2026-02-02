# 版本号功能使用说明

## 修改内容

已在 `ImageConfig_t` 结构体中添加了 `version` 字段，用于保存固件版本号。

### 结构体定义（[IAP/inc/iap_config.h](IAP/inc/iap_config.h)）

```c
typedef struct {
    uint16_t page_count;       /* Page count (number of pages used by firmware) */
    uint32_t firmware_CRC;     /* Firmware CRC32 value */
    uint32_t version;          /* Firmware version number (preserved during updates) */
} ImageConfig_t;
```

## 核心功能

### 1. 版本号在升级时自动保留

`Config_Write()` 函数已修改，在写入配置时会自动保留现有的版本号：

```c
int8_t Config_Write(const ImageConfig_t *config)
{
    // 读取现有配置，保留版本号
    ImageConfig_t temp_config;
    memcpy(&temp_config, config, sizeof(ImageConfig_t));
    
    ImageConfig_t existing_config;
    if (Config_Read(&existing_config) == 0) {
        temp_config.version = existing_config.version;  // 保留版本号
    }
    
    // 写入flash...
}
```

这意味着：
- ✅ 升级固件时，`page_count` 和 `firmware_CRC` 会更新
- ✅ 版本号 `version` 会自动保留不变
- ✅ 无需手动处理版本号保存逻辑

### 2. 新增的版本号管理函数

在 [IAP/src/iap_image.c](IAP/src/iap_image.c) 和 [IAP/inc/iap_image.h](IAP/inc/iap_image.h) 中新增了两个函数：

```c
/* 设置版本号（通常在APP程序中调用） */
int8_t Config_Set_Version(uint32_t new_version);

/* 获取当前版本号 */
uint32_t Config_Get_Version(void);
```

## APP程序使用示例

### 示例1：启动时检查版本号判断升级是否成功

```c
#include "iap_image.h"

#define EXPECTED_VERSION 0x00010002  // 版本 1.0.2

void App_Main(void)
{
    // 读取当前版本号
    uint32_t current_version = Config_Get_Version();
    
    if (current_version == EXPECTED_VERSION) {
        // 版本匹配，升级成功
        printf("Firmware version: %d.%d.%d (OK)\r\n", 
               (current_version >> 16) & 0xFF,
               (current_version >> 8) & 0xFF,
               current_version & 0xFF);
    } else {
        // 版本不匹配，可能需要重新升级
        printf("Version mismatch! Expected: 0x%08X, Got: 0x%08X\r\n",
               EXPECTED_VERSION, current_version);
        
        // 可以标记为升级失败，回滚或重新下载
        // ...
    }
    
    // 正常运行APP逻辑
    // ...
}
```

### 示例2：APP程序设置版本号

如果需要在APP程序启动后更新版本号（比如首次启动后标记版本）：

```c
#include "iap_image.h"

#define APP_VERSION 0x00010003  // 版本 1.0.3

void App_Init(void)
{
    // 获取当前版本号
    uint32_t saved_version = Config_Get_Version();
    
    if (saved_version != APP_VERSION) {
        // 版本号不匹配，更新为当前APP版本
        if (Config_Set_Version(APP_VERSION) == 0) {
            printf("Version updated to: 0x%08X\r\n", APP_VERSION);
        } else {
            printf("Failed to update version\r\n");
        }
    }
}
```

### 示例3：版本号格式建议

```c
// 版本号编码示例（32位）
#define MAKE_VERSION(major, minor, patch) \
    (((major) << 16) | ((minor) << 8) | (patch))

// 示例版本号
#define VERSION_1_0_0   MAKE_VERSION(1, 0, 0)   // 0x00010000
#define VERSION_1_0_1   MAKE_VERSION(1, 0, 1)   // 0x00010001
#define VERSION_2_3_5   MAKE_VERSION(2, 3, 5)   // 0x00020305

// 解析版本号
void Print_Version(uint32_t version)
{
    uint8_t major = (version >> 16) & 0xFF;
    uint8_t minor = (version >> 8) & 0xFF;
    uint8_t patch = version & 0xFF;
    printf("v%d.%d.%d\r\n", major, minor, patch);
}
```

## 升级流程示例

### Bootloader侧（无需修改）

Bootloader 在执行 `IAP_Update()` 时会自动保留版本号：

```c
// 在 iap.c 中
int8_t IAP_Update(void)
{
    // ... 升级过程 ...
    
    // 更新配置（版本号自动保留）
    g_config.firmware_CRC = run_crc;
    g_config.page_count = 0;
    Config_Write(&g_config);  // 这里会自动保留 version 字段
    
    // ...
}
```

### APP侧升级判断流程

```c
void Check_Firmware_Update(void)
{
    uint32_t current_version = Config_Get_Version();
    
    // 从服务器获取最新版本信息
    uint32_t server_version = Get_Latest_Version_From_Server();
    
    if (server_version > current_version) {
        printf("New version available: 0x%08X\r\n", server_version);
        
        // 1. 下载新固件到Update区
        Download_Firmware();
        
        // 2. 重启进入Bootloader执行升级
        NVIC_SystemReset();
        
        // 3. Bootloader升级后重启到APP
        // 4. APP启动时检查版本号是否匹配
    } else {
        printf("Firmware is up to date\r\n");
    }
}

void App_Startup_Check(void)
{
    uint32_t current_version = Config_Get_Version();
    uint32_t expected_version = APP_COMPILED_VERSION;  // 编译时定义的版本
    
    if (current_version != expected_version) {
        // 升级后首次启动，更新版本号
        Config_Set_Version(expected_version);
        printf("Firmware updated successfully to v%d.%d.%d\r\n",
               (expected_version >> 16) & 0xFF,
               (expected_version >> 8) & 0xFF,
               expected_version & 0xFF);
    }
}
```

## 注意事项

1. **版本号保留机制**：`Config_Write()` 会自动保留版本号，无需在Bootloader中特殊处理

2. **版本号设置时机**：
   - 推荐在APP程序首次启动时调用 `Config_Set_Version()` 设置版本号
   - 也可以在升级完成后由APP检测并更新版本号

3. **版本号比较**：
   - 简单比较：直接用 `==` 判断版本是否匹配
   - 高级比较：可以用 `>` `<` 比较版本号大小（需要合理编码）

4. **结构体大小**：
   - 原结构体：6字节（2+4）
   - 新结构体：10字节（2+4+4）
   - Flash写入仍然是16字节对齐（Quadword）

## 测试建议

1. **验证版本号保留**：
   ```c
   // Bootloader启动时
   ImageConfig_t *cfg = IAP_GetConfig();
   printf("Current version: 0x%08X\r\n", cfg->version);
   
   // 执行升级
   IAP_Update();
   
   // 升级后读取
   Config_Read(cfg);
   printf("Version after update: 0x%08X\r\n", cfg->version);
   // 应该保持不变
   ```

2. **验证APP版本检测**：
   ```c
   // APP启动时
   uint32_t ver = Config_Get_Version();
   if (ver == MY_APP_VERSION) {
       printf("Version check passed\r\n");
   } else {
       printf("Version mismatch, update required\r\n");
   }
   ```
