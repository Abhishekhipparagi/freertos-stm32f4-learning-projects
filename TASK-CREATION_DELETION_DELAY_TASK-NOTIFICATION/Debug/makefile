################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include ThirdParty/FreeRTOS/portable/MemMang/subdir.mk
-include ThirdParty/FreeRTOS/portable/GCC/ARM_CM4F/subdir.mk
-include ThirdParty/FreeRTOS/subdir.mk
-include Drivers/STM32F4xx_HAL_Driver/Src/subdir.mk
-include Core/Startup/subdir.mk
-include Core/Src/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := TASK-CREATION_DELETION_DELAY_TASK-NOTIFICATION
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
EXECUTABLES += \
TASK-CREATION_DELETION_DELAY_TASK-NOTIFICATION.elf \

MAP_FILES += \
TASK-CREATION_DELETION_DELAY_TASK-NOTIFICATION.map \

SIZE_OUTPUT += \
default.size.stdout \

OBJDUMP_LIST += \
TASK-CREATION_DELETION_DELAY_TASK-NOTIFICATION.list \


# All Target
all: main-build

# Main-build Target
main-build: TASK-CREATION_DELETION_DELAY_TASK-NOTIFICATION.elf secondary-outputs

# Tool invocations
TASK-CREATION_DELETION_DELAY_TASK-NOTIFICATION.elf TASK-CREATION_DELETION_DELAY_TASK-NOTIFICATION.map: $(OBJS) $(USER_OBJS) D:\RTOS_udemy\TASK_CREATION_DELETION_DELAY_NOTIFICATION\TASK-CREATION_DELETION_DELAY_TASK-NOTIFICATION\STM32F407VGTX_FLASH.ld makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-gcc -o "TASK-CREATION_DELETION_DELAY_TASK-NOTIFICATION.elf" @"objects.list" $(USER_OBJS) $(LIBS) -mcpu=cortex-m4 -T"D:\RTOS_udemy\TASK_CREATION_DELETION_DELAY_NOTIFICATION\TASK-CREATION_DELETION_DELAY_TASK-NOTIFICATION\STM32F407VGTX_FLASH.ld" --specs=nosys.specs -Wl,-Map="TASK-CREATION_DELETION_DELAY_TASK-NOTIFICATION.map" -Wl,--gc-sections -static --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -Wl,--start-group -lc -lm -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

default.size.stdout: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-size  $(EXECUTABLES)
	@echo 'Finished building: $@'
	@echo ' '

TASK-CREATION_DELETION_DELAY_TASK-NOTIFICATION.list: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objdump -h -S $(EXECUTABLES) > "TASK-CREATION_DELETION_DELAY_TASK-NOTIFICATION.list"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) TASK-CREATION_DELETION_DELAY_TASK-NOTIFICATION.elf TASK-CREATION_DELETION_DELAY_TASK-NOTIFICATION.list TASK-CREATION_DELETION_DELAY_TASK-NOTIFICATION.map default.size.stdout
	-@echo ' '

secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST)

fail-specified-linker-script-missing:
	@echo 'Error: Cannot find the specified linker script. Check the linker settings in the build configuration.'
	@exit 2

warn-no-linker-script-specified:
	@echo 'Warning: No linker script specified. Check the linker settings in the build configuration.'

.PHONY: all clean dependents main-build fail-specified-linker-script-missing warn-no-linker-script-specified

-include ../makefile.targets
