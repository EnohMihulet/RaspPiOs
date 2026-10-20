CPU =	 -mcpu=cortex-a76

CC =		aarch64-elf-gcc
AS =		aarch64-elf-as
OBJCOPY =	aarch64-elf-objcopy
OBJDUMP =	aarch64-elf-objdump

.DEFAULT_GOAL := img

TARGET = kernel
IMAGE = myos

C_FLAGS =	-ffreestanding -O2 -Wall -Wextra -nostdlib -fno-stack-protector -fno-pie \
		-Ilib/ -Idrivers/usart -Iarch/aarch64 -Ikernel -Idrivers/gic -Ilib/ccontainers -Ilib/cmemory
C_FLAGS +=	-MMD -MP
# Early kernel code does not enable FP/SIMD registers.
C_FLAGS +=	-mgeneral-regs-only
LD_FLAGS =	-nostdlib

C_SOURCES =	$(wildcard kernel/*.c drivers/*.c drivers/usart/*.c lib/*.c arch/aarch64/*.c drivers/gic/*.c)
ASM_SOURCES =	$(wildcard arch/aarch64/*.s kernel/*.s)
LD_SCRIPT =	linker.ld
BUILD_DIR =	build

OBJECTS =	$(ASM_SOURCES:%.s=$(BUILD_DIR)/%.s.o)
OBJECTS +=	$(C_SOURCES:%.c=$(BUILD_DIR)/%.c.o)

-include $(OBJECTS:.o=.d)

.PHONY: img elf hex all clean dump

img: $(IMAGE).img

$(IMAGE).img: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

elf: $(TARGET).elf

$(TARGET).elf: $(OBJECTS) $(LD_SCRIPT)
	$(CC) $(CPU) $(OBJECTS) $(LD_FLAGS) -T $(LD_SCRIPT) -o $@

hex: $(IMAGE).hex

$(IMAGE).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/%.s.o: %.s
	mkdir -p $(dir $@)
	$(CC) $(CPU) -c $< -o $@

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPU) $(C_FLAGS) -c $< -o $@

all: $(TARGET).elf $(IMAGE).img $(IMAGE).hex

clean:
	rm -rf $(BUILD_DIR) *.elf *.img *.hex *.map

dump: kernel.elf
	$(OBJDUMP) -D kernel.elf
