# Toolchain location - update this for your system
# if the arm-none-eabi-* is in your path, then you can leave TOOLCHAIN_LOCATION empty
TOOLCHAIN_LOCATION = # /Applications/STM32CubeIDE.app/Contents/Eclipse/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.12.3.rel1.macos64_1.0.200.202406191456/tools/bin/

# Compiler and linker with full paths
CC = $(TOOLCHAIN_LOCATION)arm-none-eabi-gcc
OBJCOPY = $(TOOLCHAIN_LOCATION)arm-none-eabi-objcopy
OBJDUMP = $(TOOLCHAIN_LOCATION)arm-none-eabi-objdump
SIZE = $(TOOLCHAIN_LOCATION)arm-none-eabi-size

# Output files
TARGET = plinkyblack.elf
BIN = plinkyblack.bin
HEX = plinkyblack.hex
LIST = plinkyblack.list
MAP = plinkyblack.map

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

OBJS := $(SRCS:../%.c=%.o)
DEPS := $(OBJS:.o=.d)

CFLAGS = -mcpu=cortex-m4 \
         -std=gnu11 \
         -DUSE_HAL_DRIVER \
         -DSTM32L476xx \
         -O3 \
         -ffunction-sections \
         -fdata-sections \
         -Wall \
         -fstack-usage \
         --specs=nano.specs \
         -mfpu=fpv4-sp-d16 \
         -mfloat-abi=hard \
         -mthumb
#-fcyclomatic-complexity 
         
LDFLAGS = -mcpu=cortex-m4 \
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

INCLUDES = -I../Drivers/CMSIS/Include \
           -I../Core/Src/tinyusb/src \
           -I../Core/Inc \
           -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include \
           -I../Drivers/STM32L4xx_HAL_Driver/Inc \
           -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy

all: $(TARGET) $(BIN) $(HEX) $(LIST) size

# Special rule for startup assembly file
Core/Startup/startup_stm32l476vgtx.o: ../Core/Startup/startup_stm32l476vgtx.s
	@echo "AS $<"
	@mkdir -p $(dir $@)
	@$(CC) -mcpu=cortex-m4 -c -x assembler-with-cpp -MMD -MP \
	--specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o $@ $<

$(OBJS): %.o: ../%.c
	@echo "CC $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c $< -o $@

# Linking
$(TARGET): $(OBJS) Core/Startup/startup_stm32l476vgtx.o
	@echo "LD $@"
	@$(CC) $(OBJS) Core/Startup/startup_stm32l476vgtx.o $(LDFLAGS) -o $@

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

size: $(TARGET)
	@echo "Size of modules:"
	@$(SIZE) $<

clean:
	rm -rf Core 
	rm -rf Drivers
	rm -f $(TARGET) $(BIN) $(HEX) $(LIST) $(MAP)
	
toolchain-info:
	@echo "Current toolchain location: $(TOOLCHAIN_LOCATION)"
	@echo "Update TOOLCHAIN_LOCATION in Makefile if this is incorrect"

.PHONY: all clean size toolchain-info

-include $(DEPS)
