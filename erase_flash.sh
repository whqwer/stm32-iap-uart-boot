#!/bin/bash
# STM32H503 Flash 擦除脚本
# 使用 GDB + OpenOCD

echo "======================================"
echo "STM32H503 Flash 全片擦除工具"
echo "======================================"

# 查找可能的 STM32CubeProgrammer CLI 路径
CUBE_PROG_PATHS=(
    "/opt/st/stm32cubeide_*/plugins/*/tools/bin/STM32_Programmer_CLI"
    "$HOME/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI"
    "/usr/local/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI"
)

CUBE_PROG=""
for path in "${CUBE_PROG_PATHS[@]}"; do
    FOUND=$(find / -path "$path" 2>/dev/null | head -1)
    if [ -n "$FOUND" ]; then
        CUBE_PROG="$FOUND"
        break
    fi
done

if [ -n "$CUBE_PROG" ]; then
    echo "找到 STM32CubeProgrammer: $CUBE_PROG"
    echo "开始擦除..."
    "$CUBE_PROG" -c port=SWD mode=UR -e all
    echo "擦除完成！"
    exit 0
fi

echo ""
echo "未找到 STM32CubeProgrammer CLI"
echo ""
echo "请使用以下方法之一手动擦除："
echo ""
echo "方法1: 使用 STM32CubeIDE"
echo "  1. 打开 STM32CubeIDE"
echo "  2. Window -> Show View -> Other -> Debug -> Memory Browser"
echo "  3. 右键 -> Export -> Export Memory to File"
echo "  4. 或使用 Run -> Debug Configurations -> Startup -> Run Commands:"
echo "     monitor flash erase_sector 0 0 last"
echo ""
echo "方法2: 安装 stlink-tools"
echo "  sudo apt install stlink-tools"
echo "  st-flash erase"
echo ""
echo "方法3: 下载 STM32CubeProgrammer"
echo "  https://www.st.com/en/development-tools/stm32cubeprog.html"
echo ""
