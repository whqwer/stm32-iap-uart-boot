cmake_minimum_required(VERSION 3.22)

project(bootloader C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(VERSION_MAJOR "" CACHE STRING "Bootloader major version (empty: derived from git tag)")
set(VERSION_MINOR "" CACHE STRING "Bootloader minor version (empty: derived from git tag)")
set(VERSION_PATCH "" CACHE STRING "Bootloader patch version (empty: derived from git tag)")
set(VERSION_STRING "" CACHE STRING "Bootloader version string used in output artifact names (empty: derived from git tag)")

# Version from git: exact tag on HEAD, else nearest ancestor tag + hash,
# else highest tag + hash, else 0.0.0-<hash>; uncommitted changes -> -dirty
find_program(GIT_EXECUTABLE git)
set(GIT_HASH "unknown")
set(GIT_TAG "")
set(GIT_TAG_EXACT FALSE)
set(GIT_DIRTY FALSE)
if(GIT_EXECUTABLE)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} status --porcelain --untracked-files=no
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_STATUS_OUT RESULT_VARIABLE GIT_STATUS_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(GIT_STATUS_RESULT EQUAL 0 AND NOT GIT_STATUS_OUT STREQUAL "")
    set(GIT_DIRTY TRUE)
  endif()
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH_OUT RESULT_VARIABLE GIT_HASH_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(GIT_HASH_RESULT EQUAL 0)
    set(GIT_HASH ${GIT_HASH_OUT})
  endif()
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --exact-match
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG_OUT RESULT_VARIABLE GIT_TAG_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(GIT_TAG_RESULT EQUAL 0)
    set(GIT_TAG ${GIT_TAG_OUT})
    set(GIT_TAG_EXACT TRUE)
  else()
    execute_process(
      COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE GIT_TAG_OUT RESULT_VARIABLE GIT_TAG_RESULT
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(NOT GIT_TAG_RESULT EQUAL 0)
      execute_process(
        COMMAND ${GIT_EXECUTABLE} tag --sort=-v:refname
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG_OUT RESULT_VARIABLE GIT_TAG_RESULT
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
      if(GIT_TAG_RESULT EQUAL 0 AND GIT_TAG_OUT)
        string(REGEX REPLACE "\n.*" "" GIT_TAG_OUT "${GIT_TAG_OUT}")
      else()
        set(GIT_TAG_OUT "")
      endif()
    endif()
    set(GIT_TAG ${GIT_TAG_OUT})
  endif()
endif()

if(GIT_TAG MATCHES "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)")
  set(GIT_VERSION_MAJOR ${CMAKE_MATCH_1})
  set(GIT_VERSION_MINOR ${CMAKE_MATCH_2})
  set(GIT_VERSION_PATCH ${CMAKE_MATCH_3})
else()
  if(GIT_TAG)
    message(WARNING "Git tag '${GIT_TAG}' is not in <major>.<minor>.<patch> form, using 0.0.0")
  endif()
  set(GIT_VERSION_MAJOR 0)
  set(GIT_VERSION_MINOR 0)
  set(GIT_VERSION_PATCH 0)
endif()

if(VERSION_MAJOR STREQUAL "")
  set(VERSION_MAJOR ${GIT_VERSION_MAJOR})
endif()
if(VERSION_MINOR STREQUAL "")
  set(VERSION_MINOR ${GIT_VERSION_MINOR})
endif()
if(VERSION_PATCH STREQUAL "")
  set(VERSION_PATCH ${GIT_VERSION_PATCH})
endif()

set(PROJECT_TARGET bootloader)
set(FIRMWARE_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
if(NOT VERSION_STRING)
  if(GIT_DIRTY)
    set(VERSION_STRING ${FIRMWARE_VERSION}-dirty)
  elseif(GIT_TAG_EXACT)
    set(VERSION_STRING ${FIRMWARE_VERSION})
  else()
    set(VERSION_STRING ${FIRMWARE_VERSION}-${GIT_HASH})
  endif()
endif()
message(STATUS "Bootloader version: ${VERSION_STRING} (tag: '${GIT_TAG}', hash: ${GIT_HASH}, dirty: ${GIT_DIRTY})")

set(OUTPUT_BASENAME ${PROJECT_TARGET}-${VERSION_STRING})
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/STM32H503CBTX_FLASH.ld)
set(MAP_FILE ${CMAKE_BINARY_DIR}/${OUTPUT_BASENAME}.map)

set(PROJECT_SOURCES
  Core/Src/gpdma.c
  Core/Src/gpio.c
  Core/Src/main.c
  Core/Src/spi.c
  Core/Src/stm32h5xx_hal_msp.c
  Core/Src/stm32h5xx_it.c
  Core/Src/syscalls.c
  Core/Src/sysmem.c
  Core/Src/system_stm32h5xx.c
  Core/Src/usart.c
  Core/Startup/startup_stm32h503cbtx.s
  HARDWARE/LCD/Src/lcd_boot.c
  HARDWARE/LCD/Src/lcd_init.c
  HARDWARE/LCD/Src/lcdfont_boot.c
  HARDWARE/LCD/Src/spi_func.c
  IAP/src/common.c
  IAP/src/iap.c
  IAP/src/iap_image.c
  IAP/src/protocol.c
  IAP/src/stmflash.c
  Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal.c
  Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_cortex.c
  Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_dma.c
  Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_dma_ex.c
  Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_exti.c
  Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash.c
  Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash_ex.c
  Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_gpio.c
  Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pwr.c
  Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pwr_ex.c
  Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rcc.c
  Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rcc_ex.c
  Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_spi.c
  Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_spi_ex.c
  Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_uart.c
  Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_uart_ex.c
)

add_executable(${PROJECT_TARGET} ${PROJECT_SOURCES})

set_target_properties(${PROJECT_TARGET} PROPERTIES OUTPUT_NAME ${OUTPUT_BASENAME} SUFFIX .elf)

target_include_directories(${PROJECT_TARGET} PRIVATE
  Core/Inc
  HARDWARE/LCD/Inc
  IAP/inc
  Drivers/STM32H5xx_HAL_Driver/Inc
  Drivers/STM32H5xx_HAL_Driver/Inc/Legacy
  Drivers/CMSIS/Device/ST/STM32H5xx/Include
  Drivers/CMSIS/Include
)

target_compile_definitions(${PROJECT_TARGET} PRIVATE
  USE_HAL_DRIVER
  STM32H503xx
  VERSION_MAJOR=${VERSION_MAJOR}
  VERSION_MINOR=${VERSION_MINOR}
  VERSION_PATCH=${VERSION_PATCH}
  $<$<CONFIG:Debug>:DEBUG>
)

target_compile_options(${PROJECT_TARGET} PRIVATE
  -mcpu=cortex-m33
  -mfpu=fpv5-sp-d16
  -mfloat-abi=hard
  -mthumb
  --specs=nano.specs
  -ffunction-sections
  -fdata-sections
  -Wall
  $<$<COMPILE_LANGUAGE:C>:-Os>
  $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:Debug>>:-g3>
  $<$<COMPILE_LANGUAGE:C>:-fstack-usage>
  $<$<COMPILE_LANGUAGE:C>:-fcyclomatic-complexity>
  $<$<AND:$<COMPILE_LANGUAGE:ASM>,$<CONFIG:Debug>>:-g3>
  $<$<COMPILE_LANGUAGE:ASM>:-x>
  $<$<COMPILE_LANGUAGE:ASM>:assembler-with-cpp>
)

target_link_options(${PROJECT_TARGET} PRIVATE
  -mcpu=cortex-m33
  -mfpu=fpv5-sp-d16
  -mfloat-abi=hard
  -mthumb
  --specs=nosys.specs
  --specs=nano.specs
  -T${LINKER_SCRIPT}
  -Wl,-Map=${MAP_FILE}
  -Wl,--gc-sections
  -static
  -Wl,--start-group
  -lc
  -lm
  -Wl,--end-group
)

find_program(CMAKE_SIZE arm-none-eabi-size REQUIRED)
find_program(CMAKE_OBJCOPY arm-none-eabi-objcopy REQUIRED)

add_custom_command(TARGET ${PROJECT_TARGET} POST_BUILD
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_TARGET}>
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_TARGET}> ${CMAKE_BINARY_DIR}/${OUTPUT_BASENAME}.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_TARGET}> ${CMAKE_BINARY_DIR}/${OUTPUT_BASENAME}.bin
  BYPRODUCTS
    ${CMAKE_BINARY_DIR}/${OUTPUT_BASENAME}.hex
    ${CMAKE_BINARY_DIR}/${OUTPUT_BASENAME}.bin
)
