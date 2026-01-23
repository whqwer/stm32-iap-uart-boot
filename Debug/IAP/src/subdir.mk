################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../IAP/src/common.c \
../IAP/src/iap.c \
../IAP/src/protocol.c \
../IAP/src/stmflash.c 

OBJS += \
./IAP/src/common.o \
./IAP/src/iap.o \
./IAP/src/protocol.o \
./IAP/src/stmflash.o 

C_DEPS += \
./IAP/src/common.d \
./IAP/src/iap.d \
./IAP/src/protocol.d \
./IAP/src/stmflash.d 


# Each subdirectory must supply rules for building sources it contributes
IAP/src/%.o IAP/src/%.su IAP/src/%.cyclo: ../IAP/src/%.c IAP/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32H503xx -c -I../Core/Inc -I../Drivers/STM32H5xx_HAL_Driver/Inc -I../Drivers/STM32H5xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32H5xx/Include -I../Drivers/CMSIS/Include -I"/home/wh/Documents/IAP/newboot/stm32-iap-uart-boot/IAP/inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-IAP-2f-src

clean-IAP-2f-src:
	-$(RM) ./IAP/src/common.cyclo ./IAP/src/common.d ./IAP/src/common.o ./IAP/src/common.su ./IAP/src/iap.cyclo ./IAP/src/iap.d ./IAP/src/iap.o ./IAP/src/iap.su ./IAP/src/protocol.cyclo ./IAP/src/protocol.d ./IAP/src/protocol.o ./IAP/src/protocol.su ./IAP/src/stmflash.cyclo ./IAP/src/stmflash.d ./IAP/src/stmflash.o ./IAP/src/stmflash.su

.PHONY: clean-IAP-2f-src

