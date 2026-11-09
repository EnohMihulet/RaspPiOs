CPU =	 -mcpu=cortex-a76

CC      = aarch64-elf-gcc
AS      = aarch64-elf-as
OBJCOPY = aarch64-elf-objcopy
OBJDUMP = aarch64-elf-objdump

HOST_CC   ?= cc
ET_DIR     = tests/framework/Embedded-Test/et
HOST_TEST_NAMES = test_fdt test_memmap test_page_alloc
HOST_TESTS = $(addprefix $(BUILD_DIR)/tests/,$(HOST_TEST_NAMES))
HOST_CFLAGS ?= -std=c11 -Wall -Wextra
HOST_CPPFLAGS ?= -iquote lib -iquote lib/ccontainers -iquote lib/cmemory -iquote kernel

.DEFAULT_GOAL := img

TARGET = kernel
IMAGE  = myos

C_FLAGS   = -ffreestanding -O2 -Wall -Wextra -nostdlib -fno-stack-protector -fno-pie \
		-Ilib/ -Idrivers/usart -Iarch/aarch64 -Ikernel -Idrivers/gic -Ilib/ccontainers -Ilib/cmemory
C_FLAGS  += -MMD -MP
C_FLAGS  += -mgeneral-regs-only
LD_FLAGS  = -nostdlib

C_SOURCES   = $(wildcard kernel/*.c drivers/*.c drivers/usart/*.c lib/*.c arch/aarch64/*.c drivers/gic/*.c)
ASM_SOURCES = $(wildcard arch/aarch64/*.s kernel/*.s)
LD_SCRIPT   = linker.ld
BUILD_DIR   = build

OBJECTS  = $(ASM_SOURCES:%.s=$(BUILD_DIR)/%.s.o)
OBJECTS += $(C_SOURCES:%.c=$(BUILD_DIR)/%.c.o)

-include $(OBJECTS:.o=.d)

.PHONY: img elf hex all clean dump test host-tests

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

test: host-tests

host-tests: $(HOST_TESTS)
	@status=0; for test in $(HOST_TESTS); do \
		./$$test || status=1; \
	done; exit $$status

# Register each suite above and list its implementation/header dependencies here.
$(BUILD_DIR)/tests/test_convert: lib/convert.c lib/convert.h
$(BUILD_DIR)/tests/test_fdt: lib/fdt.h
$(BUILD_DIR)/tests/test_fdt $(BUILD_DIR)/tests/test_memmap: tests/host/fdt_builder.h
$(BUILD_DIR)/tests/test_memmap: kernel/memmap.c kernel/memmap.h lib/fdt.h lib/ccontainers/dynamic_array.h kernel/alloc.h lib/cmemory/freelist.h lib/align.h kernel/page_allocator.h
$(BUILD_DIR)/tests/test_page_alloc: kernel/page_allocator.c kernel/page_allocator.h kernel/memmap.c kernel/memmap.h lib/fdt.h lib/ccontainers/dynamic_array.h kernel/alloc.h lib/cmemory/freelist.h lib/align.h tests/host/fdt_builder.h

$(HOST_TESTS): $(BUILD_DIR)/tests/%: tests/host/%.c \
		$(ET_DIR)/et.c $(ET_DIR)/et.h $(ET_DIR)/et_host.c
	mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -I$(ET_DIR) $(filter %.c,$^) -o $@


dump: kernel.elf
	$(OBJDUMP) -D kernel.elf
