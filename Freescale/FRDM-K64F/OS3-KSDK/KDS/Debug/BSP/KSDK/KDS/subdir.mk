################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_UPPER_SRCS += \
C:/Users/pierg/workspace.kds/Freescale/FRDM-K64F/BSP/KSDK/KDS/startup_MK64F12.S 

OBJS += \
./BSP/KSDK/KDS/startup_MK64F12.o 

S_UPPER_DEPS += \
./BSP/KSDK/KDS/startup_MK64F12.d 


# Each subdirectory must supply rules for building sources it contributes
BSP/KSDK/KDS/startup_MK64F12.o: C:/Users/pierg/workspace.kds/Freescale/FRDM-K64F/BSP/KSDK/KDS/startup_MK64F12.S
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM GNU Assembler'
	arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -O0 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections  -g -x assembler-with-cpp -DDEBUG -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


