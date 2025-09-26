PROJECT_NAME := template
PROJECT_VERSION := f33

# Build info
BUILD_DIR := build
STM32_BUILD_DIR := $(BUILD_DIR)/stm32

# Cross-compilation tooling
STM32_PREFIX := arm-none-eabi
STM32_CC := $(STM32_PREFIX)-gcc
STM32_ASM := $(STM32_PREFIX)-gcc
STM32_LD := $(STM32_PREFIX)-gcc
STM32_GDB := $(STM32_PREFIX)-gdb
STM32_OBJCOPY := $(STM32_PREFIX)-objcopy
STM32_OBJDUMP := $(STM32_PREFIX)-objdump

# Cross-compilation options
STM32_COMMON_FLAGS := -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -D USE_HAL_DRIVER -D STM32G441xx
STM32_CC_FLAGS := $(STM32_COMMON_FLAGS) -ffreestanding -ffunction-sections -fdata-sections -Wall -Wextra -Werror=implicit-function-declaration -g
STM32_ASM_FLAGS := $(STM32_CC_FLAGS)
STM32_LD_SCRIPT := STM32G441xBxx_FLASH.ld
STM32_LD_FLAGS := $(STM32_COMMON_FLAGS) -static -Wl,--gc-sections -T $(STM32_LD_SCRIPT) -specs=nano.specs -specs=nosys.specs


# Sources
APP_DIR := src/app
APP_SRCS := $(shell find $(APP_DIR) -type f -name "*.c")
APP_INCLUDE := -I $(APP_DIR)
STM32_APP_OBJS := $(APP_SRCS:$(APP_DIR)/%=$(STM32_BUILD_DIR)/obj/app/%.o)

DRIVER_DIR := src/driver
DRIVER_SRCS := $(shell find $(DRIVER_DIR) -type f -name "*.c")
DRIVER_INCLUDE := -I $(DRIVER_DIR)
STM32_DRIVER_OBJS := $(DRIVER_SRCS:$(DRIVER_DIR)/%=$(STM32_BUILD_DIR)/obj/driver/%.o)

# Libraries
STM32CUBE_DIR := ../STM32CubeG4
STM32CUBE_HAL_DIR := $(STM32CUBE_DIR)/Drivers/STM32G4xx_HAL_Driver
STM32CUBE_CMSIS_DIR := $(STM32CUBE_DIR)/Drivers/CMSIS/Device/ST/STM32G4xx
STM32CUBE_SRC_DIRS := $(STM32CUBE_HAL_DIR)/Src
STM32CUBE_SRCS := $(shell find $(STM32CUBE_SRC_DIRS) -maxdepth 1 -type f -name "*.c") $(STM32CUBE_CMSIS_DIR)/Source/Templates/system_stm32g4xx.c
STM32CUBE_ASMS := src/startup_stm32g441xx.s
STM32CUBE_INCLUDES := $(STM32CUBE_HAL_DIR)/Inc $(STM32CUBE_DIR)/Drivers/CMSIS/Include $(STM32CUBE_CMSIS_DIR)/Include
STM32CUBE_INCLUDES := $(foreach d, $(STM32CUBE_INCLUDES),-I $d)
STM32CUBE_OBJS := $(STM32CUBE_SRCS:$(STM32CUBE_DIR)/%=$(STM32_BUILD_DIR)/obj/stm32cube/%.o) $(STM32_BUILD_DIR)/obj/stm32cube/startup_stm32g441xx.s.o

FREERTOS_DIR := ../FreeRTOS-Kernel
FREERTOS_SRC_DIRS := $(FREERTOS_DIR) $(FREERTOS_DIR)/portable/GCC/ARM_CM4F
FREERTOS_SRCS := $(shell find $(FREERTOS_SRC_DIRS) -maxdepth 1 -type f -name "*.c") $(FREERTOS_DIR)/portable/MemMang/heap_4.c
FREERTOS_INCLUDES := $(FREERTOS_DIR)/include $(FREERTOS_DIR)/portable/GCC/ARM_CM4F
FREERTOS_INCLUDES := $(foreach d, $(FREERTOS_INCLUDES),-I $d)
FREERTOS_OBJS := $(FREERTOS_SRCS:$(FREERTOS_DIR)/%=$(STM32_BUILD_DIR)/obj/freertos/%.o)

RTT_DIR := ../RTT
ifneq ("$(wildcard $(RTT_DIR))","")
RTT_SRCS := $(RTT_DIR)/RTT/SEGGER_RTT.c $(RTT_DIR)/Syscalls/SEGGER_RTT_Syscalls_GCC.c $(RTT_DIR)/RTT/SEGGER_RTT_printf.c
RTT_INCLUDES := $(addprefix -I, ./src) $(addprefix -I, $(RTT_DIR)/RTT)
RTT_OBJS := $(RTT_SRCS:$(RTT_DIR)/%=$(STM32_BUILD_DIR)/obj/rtt/%.o)
endif

DBC_DIR := ../Formula-DBC
ifneq ("$(wildcard $(DBC_DIR))","")
DBC_SRCS := $(shell find -L $(DBC_DIR)/c_files -name "*.c")
DBC_INCLUDES := -I $(DBC_DIR)/c_files
DBC_OBJS := $(DBC_SRCS:$(DBC_DIR)/c_files/%=$(STM32_BUILD_DIR)/obj/formula_dbc/%.o)
endif

CORE_DIR := ../core/src/driver
CORE_SRCS := $(shell find $(CORE_DIR)/Src -type f -name "*.c")
CORE_INCLUDES := -I $(CORE_DIR)/Inc $(STM32CUBE_INCLUDES) $(FREERTOS_INCLUDES) $(RTT_INCLUDES)
CORE_INCLUDES := $(foreach d, $(CORE_INCLUDES),-I $d)
CORE_OBJS :=  $(CORE_SRCS:$(CORE_DIR)/%=$(STM32_BUILD_DIR)/obj/core/%.o)

OUTNAME := $(STM32_BUILD_DIR)/$(PROJECT_NAME)-$(PROJECT_VERSION)

# Compilation targets
.PHONY: all
all: main

.PHONY: main
main: $(OUTNAME).elf $(OUTNAME).bin $(OUTNAME).ihex

# Main executable
$(OUTNAME).bin: $(OUTNAME).elf
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(STM32_OBJCOPY) -O binary $< $@

$(OUTNAME).elf: $(STM32_APP_OBJS) $(STM32_DRIVER_OBJS) $(STM32CUBE_OBJS) $(FREERTOS_OBJS) $(CORE_OBJS) $(RTT_OBJS) $(DBC_OBJS)
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(STM32_LD) $(STM32_LD_FLAGS) $^ -o $@ -lc -lm

$(OUTNAME).ihex: $(OUTNAME).elf
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(STM32_OBJCOPY) -O ihex $< $@

# application objects
$(STM32_BUILD_DIR)/obj/app/%.c.o: $(APP_DIR)/%.c
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(STM32_CC) $(STM32_CC_FLAGS) -I src $(APP_INCLUDE) $(DRIVER_INCLUDE) $(FREERTOS_INCLUDES) $(CORE_INCLUDES) $(RTT_INCLUDES) $(DBC_INCLUDES) -c $< -o $@

# driver objects
$(STM32_BUILD_DIR)/obj/driver/%.c.o: $(DRIVER_DIR)/%.c
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(STM32_CC) $(STM32_CC_FLAGS) -I src $(DRIVER_INCLUDE) $(STM32CUBE_INCLUDES) $(CORE_INCLUDES) $(RTT_INCLUDES) $(DBC_INCLUDES) -c $< -o $@

# stm32cube objects
$(STM32_BUILD_DIR)/obj/stm32cube/%.c.o: $(STM32CUBE_DIR)/%.c
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(STM32_CC) $(STM32_CC_FLAGS) -I src $(STM32CUBE_INCLUDES) -c $< -o $@

$(STM32_BUILD_DIR)/obj/stm32cube/startup_stm32g441xx.s.o: src/startup_stm32g441xx.s
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(STM32_CC) $(STM32_ASM_FLAGS) -c $< -o $@

# freertos objects
$(STM32_BUILD_DIR)/obj/freertos/%.c.o: $(FREERTOS_DIR)/%.c
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(STM32_CC) $(STM32_CC_FLAGS) -I src $(FREERTOS_INCLUDES) -c $< -o $@

# core objects
$(STM32_BUILD_DIR)/obj/core/%.c.o: $(CORE_DIR)/%.c
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(STM32_CC) $(STM32_CC_FLAGS) -I src $(STM32CUBE_INCLUDES) $(CORE_INCLUDES) -c $< -o $@

# dbc objects
$(STM32_BUILD_DIR)/obj/formula_dbc/%.c.o: $(DBC_DIR)/c_files/%.c
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(STM32_CC) $(STM32_CC_FLAGS) -I src $(DBC_INCLUDES) -c $< -o $@

# RTT objects
$(STM32_BUILD_DIR)/obj/rtt/%.c.o: $(RTT_DIR)/%.c
	@[ -d $(@D) ] || mkdir -p $(@D)
	$(STM32_CC) $(STM32_CC_FLAGS) -I src $(RTT_INCLUDES) -c $< -o $@

# Misc targets
.PHONY: clean
clean:
	rm -r $(BUILD_DIR)

.PHONY: clean-user
clean-user:
	rm -r $(BUILD_DIR)/stm32/obj/app
	rm -r $(BUILD_DIR)/stm32/obj/core
