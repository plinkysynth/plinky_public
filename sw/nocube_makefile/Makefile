
# Toolchain location - update this for your system
# if the arm-none-eabi-* is in your path, then you can leave TOOLCHAIN_LOCATION empty
# you can also set this from the commandline like
# make TOOLCHAIN_LOCATION=/path/to/toolchain
TOOLCHAIN_LOCATION ?= /Applications/STM32CubeIDE.app/Contents/Eclipse/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.12.3.rel1.macos64_1.1.0.202410301201/tools/bin/

# set this to the location of the stm32 cube programmer cli app,
# and then the 'program' pseudo target will flash your plinky
# you can also set this from the commandline like
# make STM32_CUBE_PROGRAMMER=/path/to/STM32_Programmer_CLI
STM32_CUBE_PROGRAMMER ?= /Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin/STM32_Programmer_CLI

# Compiler and linker with full paths
CC = $(TOOLCHAIN_LOCATION)arm-none-eabi-gcc
OBJCOPY = $(TOOLCHAIN_LOCATION)arm-none-eabi-objcopy
OBJDUMP = $(TOOLCHAIN_LOCATION)arm-none-eabi-objdump
SIZE = $(TOOLCHAIN_LOCATION)arm-none-eabi-size

# Output files
# Set default build type if not specified
BUILD_TYPE ?= RELEASE

BUILD_DIR := $(BUILD_TYPE)
$(shell mkdir -p $(BUILD_DIR))

TARGET = $(BUILD_DIR)/plinkyblack.elf
BIN = $(BUILD_DIR)/plinkyblack.bin
HEX = $(BUILD_DIR)/plinkyblack.hex
LIST = $(BUILD_DIR)/plinkyblack.list
MAP = $(BUILD_DIR)/plinkyblack.map

# Source files
SRCS = \
	   ../Core/Src/main.c \
	../Core/Src/gfx.c \
	../Core/Src/lis2dh12_reg.c \
	../Core/Src/plinky.c \
	../Core/Src/stm32l4xx_hal_msp.c \
	../Core/Src/stm32l4xx_it.c \
	../Core/Src/syscalls.c \
	../Core/Src/sysmem.c \
	../Core/Src/system_stm32l4xx.c \
	../Core/Src/tinyusb/src/tusb.c \
	../Core/Src/tinyusb/src/usb_descriptors.c \
	../Core/Src/tinyusb/src/usbmidi.c \
	../Core/Src/tinyusb/src/class/midi/midi_device.c \
	../Core/Src/tinyusb/src/class/vendor/vendor_device.c \
	../Core/Src/tinyusb/src/common/tusb_fifo.c \
	../Core/Src/tinyusb/src/device/usbd.c \
	../Core/Src/tinyusb/src/device/usbd_control.c \
	../Core/Src/tinyusb/src/portable/st/synopsys/dcd_synopsys.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_adc.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_adc_ex.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_cortex.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_dac.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_dma.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_flash.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_flash_ex.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_gpio.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_i2c.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_i2c_ex.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_pcd.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_pcd_ex.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_pwr_ex.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_rcc.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_rcc_ex.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_sai.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_spi.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_tim.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_tim_ex.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_tsc.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_uart.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_uart_ex.c \
	../Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_ll_usb.c

OBJS := $(SRCS:../%.c=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)


# Base CFLAGS and LDFLAGS
CFLAGS_COMMON = -mcpu=cortex-m4 \
    -std=gnu11 \
    -DUSE_HAL_DRIVER \
    -DSTM32L476xx \
    -ffunction-sections \
    -fdata-sections \
    -Wall \
    -fstack-usage \
    --specs=nano.specs \
    -mfpu=fpv4-sp-d16 \
    -mfloat-abi=hard \
    -mthumb

LDFLAGS_COMMON = -mcpu=cortex-m4 \
    -T"../STM32L476VGTX_FLASH.ld" \
    --specs=nosys.specs \
    -Wl,-Map="$(MAP)" \
    -Wl,--gc-sections \
    -static \
    --specs=nano.specs \
    -mfpu=fpv4-sp-d16 \
    -mfloat-abi=hard \
    -mthumb \
    -Wl,--start-group -lc -lm -Wl,--end-group

# Flags for release build
ifeq ($(BUILD_TYPE), RELEASE)
CFLAGS = $(CFLAGS_COMMON) -O3 -DNDEBUG
LDFLAGS = $(LDFLAGS_COMMON)
endif

# Flags for debug build
ifeq ($(BUILD_TYPE), DEBUG)
CFLAGS = $(CFLAGS_COMMON) -Og -g3 -D_DEBUG -DDEBUG -gdwarf-2
LDFLAGS = $(LDFLAGS_COMMON)
endif

# Print build type
$(info Building in $(BUILD_TYPE) mode)

INCLUDES = -I../Drivers/CMSIS/Include \
	    -I../Core/Src/tinyusb/src \
	    -I../Core/Inc \
	    -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include \
	    -I../Drivers/STM32L4xx_HAL_Driver/Inc \
	    -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy

all: $(TARGET) $(BIN) $(HEX) $(LIST) size

# Special rule for startup assembly file
$(BUILD_DIR)/Core/Startup/startup_stm32l476vgtx.o: ../Core/Startup/startup_stm32l476vgtx.s
	@echo "AS $<"
	@mkdir -p $(dir $@)
	@$(CC) -mcpu=cortex-m4 -c -x assembler-with-cpp -MMD -MP \
	--specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o $@ $<

$(BUILD_DIR)/%.o: ../%.c
	@echo "CC $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c $< -o $@

# $(OBJS): %.o: ../%.c
# 	@echo "CC $<"
# 	@mkdir -p $(dir $@)
# 	@$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c $< -o $@

# Linking
$(TARGET): $(OBJS) $(BUILD_DIR)/Core/Startup/startup_stm32l476vgtx.o
	@echo "LD $@"
	@$(CC) $(OBJS) $(BUILD_DIR)/Core/Startup/startup_stm32l476vgtx.o $(LDFLAGS) -o $@

# Post-build steps
$(BIN): $(TARGET)
	@echo "Creating binary $@"
	@$(OBJCOPY) -O binary $< $@

$(HEX): $(TARGET)
	@echo "Creating hex $@"
	@$(OBJCOPY) -O ihex $< $@

$(LIST): $(TARGET)
	@echo "Creating listing $@"
	@$(OBJDUMP) -h -S $< > $@

program: $(HEX)
ifeq ($(STM32_CUBE_PROGRAMMER),)
	$(error STM32_CUBE_PROGRAMMER is not set. Please set the path to STM32_Programmer_CLI)
else
	@echo "Flashing $<"
	@$(STM32_CUBE_PROGRAMMER) -c port=SWD -w $< -v -rst
endif

size: $(TARGET)
	@echo "Size of modules:"
	@$(SIZE) $<

clean:
	rm -rf $(BUILD_DIR) 
	
toolchain-info:
	@echo "Current toolchain location: $(TOOLCHAIN_LOCATION)"
	@echo "Update TOOLCHAIN_LOCATION in Makefile if this is incorrect"

.PHONY: all clean size toolchain-info

-include $(DEPS)
