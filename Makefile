# Commands:
#   make / make img    Build the normal kernel image.
#   make elf / hex    Build the normal kernel ELF / Intel HEX file.
#   make all          Build all three normal kernel formats.
#   make host-tests   Build and run tests on this computer.
#   make target-tests Build the test image to run on the Raspberry Pi.
#   make dump         Disassemble the normal kernel ELF.
#   make clean        Remove generated build files.

.DEFAULT_GOAL := img
.PHONY: img elf hex all test host-tests target-tests dump clean

# -----------------------------------------------------------------------------
# Tools and shared Raspberry Pi compiler settings
# -----------------------------------------------------------------------------
# TARGET means the Raspberry Pi; HOST means the computer running make.
# Command-line assignments can override these, e.g. make HOST_CC=clang host-tests.
TARGET_CC      = aarch64-elf-gcc
TARGET_OBJCOPY = aarch64-elf-objcopy
TARGET_OBJDUMP = aarch64-elf-objdump
HOST_CC        = cc

TARGET_CPU_FLAGS = -mcpu=cortex-a76
TARGET_CPPFLAGS = \
	-Ilib \
	-Idrivers/usart \
	-Iarch/aarch64 \
	-Ikernel \
	-Idrivers/gic \
	-Ilib/ccontainers \
	-Ilib/cmemory

# CPPFLAGS holds include paths; CFLAGS holds C compilation options.
# -MMD -MP generates .d files so header edits trigger recompilation.
TARGET_CFLAGS = -ffreestanding -O2 -Wall -Wextra -nostdlib \
	-fno-stack-protector -fno-pie -mgeneral-regs-only -MMD -MP
TARGET_LDFLAGS = -nostdlib
TARGET_LINKER_SCRIPT = linker.ld

# -----------------------------------------------------------------------------
# Directories and output files
# -----------------------------------------------------------------------------
BUILD_DIR = build
ET_DIR = tests/framework/Embedded-Test/et

# Separate object directories prevent reuse of objects with different flags.
KERNEL_BUILD_DIR      = $(BUILD_DIR)/kernel
HOST_TEST_BUILD_DIR   = $(BUILD_DIR)/tests
TARGET_TEST_BUILD_DIR = $(BUILD_DIR)/target-tests

KERNEL_ELF   = kernel.elf
KERNEL_IMAGE = myos.img
KERNEL_HEX   = myos.hex

TARGET_TEST_ELF   = kernel_test.elf
TARGET_TEST_IMAGE = test_os.img

# -----------------------------------------------------------------------------
# Normal kernel sources and objects
# -----------------------------------------------------------------------------
# wildcard lists the existing files matching these patterns.
KERNEL_C_SOURCES = $(wildcard \
	kernel/*.c \
	drivers/*.c \
	drivers/usart/*.c \
	lib/*.c \
	arch/aarch64/*.c \
	drivers/gic/*.c)
KERNEL_ASM_SOURCES = $(wildcard arch/aarch64/*.s kernel/*.s)

# These substitutions turn source paths into object paths, for example:
# kernel/alloc.c -> build/kernel/kernel/alloc.c.o
KERNEL_OBJECTS  = $(KERNEL_ASM_SOURCES:%.s=$(KERNEL_BUILD_DIR)/%.s.o)
KERNEL_OBJECTS += $(KERNEL_C_SOURCES:%.c=$(KERNEL_BUILD_DIR)/%.c.o)

# -----------------------------------------------------------------------------
# Raspberry Pi test sources and objects
# -----------------------------------------------------------------------------
# Use the kernel implementation, but replace its entry point with the test one.
# The project-specific ET adapter lives in tests/target; et_host.c is host-only.
TARGET_TEST_C_SOURCES = $(filter-out kernel/kernel.c,$(KERNEL_C_SOURCES))
TARGET_TEST_C_SOURCES += $(wildcard tests/target/*.c)
TARGET_TEST_C_SOURCES += $(ET_DIR)/et.c
TARGET_TEST_ASM_SOURCES = $(KERNEL_ASM_SOURCES)

TARGET_TEST_CPPFLAGS = $(TARGET_CPPFLAGS) -I$(ET_DIR)
TARGET_TEST_CFLAGS = $(TARGET_CFLAGS)

TARGET_TEST_OBJECTS  = $(TARGET_TEST_ASM_SOURCES:%.s=$(TARGET_TEST_BUILD_DIR)/%.s.o)
TARGET_TEST_OBJECTS += $(TARGET_TEST_C_SOURCES:%.c=$(TARGET_TEST_BUILD_DIR)/%.c.o)

# -----------------------------------------------------------------------------
# Host test settings
# -----------------------------------------------------------------------------
# Add a suite name here and its implementation/header dependencies below.
HOST_TEST_NAMES = test_fdt test_memmap test_page_alloc
HOST_TEST_BINARIES = $(addprefix $(HOST_TEST_BUILD_DIR)/,$(HOST_TEST_NAMES))

HOST_TEST_CFLAGS = -std=c11 -Wall -Wextra
HOST_TEST_CPPFLAGS = \
	-iquote lib \
	-iquote lib/ccontainers \
	-iquote lib/cmemory \
	-iquote kernel

# -----------------------------------------------------------------------------
# Normal kernel build rules
# -----------------------------------------------------------------------------
# In recipes: $@ = output filename, $< = first prerequisite, $^ = all prerequisites.
# In pattern rules, % matches the source path without its suffix.
# $(dir $@) gives the output directory; mkdir -p creates it if needed.
img: $(KERNEL_IMAGE)
elf: $(KERNEL_ELF)
hex: $(KERNEL_HEX)
all: $(KERNEL_ELF) $(KERNEL_IMAGE) $(KERNEL_HEX)

$(KERNEL_IMAGE): $(KERNEL_ELF)
	$(TARGET_OBJCOPY) -O binary $< $@

$(KERNEL_HEX): $(KERNEL_ELF)
	$(TARGET_OBJCOPY) -O ihex $< $@

$(KERNEL_ELF): $(KERNEL_OBJECTS) $(TARGET_LINKER_SCRIPT)
	$(TARGET_CC) $(TARGET_CPU_FLAGS) $(KERNEL_OBJECTS) $(TARGET_LDFLAGS) -T $(TARGET_LINKER_SCRIPT) -o $@

$(KERNEL_BUILD_DIR)/%.s.o: %.s
	mkdir -p $(dir $@)
	$(TARGET_CC) $(TARGET_CPU_FLAGS) -c $< -o $@

$(KERNEL_BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(TARGET_CC) $(TARGET_CPU_FLAGS) $(TARGET_CPPFLAGS) $(TARGET_CFLAGS) -c $< -o $@

# -----------------------------------------------------------------------------
# Raspberry Pi test build rules (build only; boot the image on the Pi to run)
# -----------------------------------------------------------------------------
target-tests: $(TARGET_TEST_IMAGE)

$(TARGET_TEST_IMAGE): $(TARGET_TEST_ELF)
	$(TARGET_OBJCOPY) -O binary $< $@

$(TARGET_TEST_ELF): $(TARGET_TEST_OBJECTS) $(TARGET_LINKER_SCRIPT)
	$(TARGET_CC) $(TARGET_CPU_FLAGS) $(TARGET_TEST_OBJECTS) $(TARGET_LDFLAGS) -T $(TARGET_LINKER_SCRIPT) -o $@

$(TARGET_TEST_BUILD_DIR)/%.s.o: %.s
	mkdir -p $(dir $@)
	$(TARGET_CC) $(TARGET_CPU_FLAGS) -c $< -o $@

$(TARGET_TEST_BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(TARGET_CC) $(TARGET_CPU_FLAGS) $(TARGET_TEST_CPPFLAGS) $(TARGET_TEST_CFLAGS) -c $< -o $@

# -----------------------------------------------------------------------------
# Host test build and run rules
# -----------------------------------------------------------------------------
test: host-tests

# Run every suite, then report failure if any suite failed.
# $$ passes a literal $ to the shell (for its variables).
host-tests: $(HOST_TEST_BINARIES)
	@status=0; for test in $(HOST_TEST_BINARIES); do \
		./$$test || status=1; \
	done; exit $$status

# Per-suite dependencies. Headers trigger rebuilds but are not compiled directly.
$(HOST_TEST_BUILD_DIR)/test_convert: lib/convert.c lib/convert.h

$(HOST_TEST_BUILD_DIR)/test_fdt: \
	lib/fdt.h \
	tests/host/fdt_builder.h

$(HOST_TEST_BUILD_DIR)/test_memmap: \
	kernel/memmap.c kernel/memmap.h \
	kernel/alloc.h kernel/page_allocator.h \
	lib/fdt.h lib/align.h \
	lib/ccontainers/dynamic_array.h lib/cmemory/freelist.h \
	tests/host/fdt_builder.h

# This extra include path applies only to the page allocator suite.
$(HOST_TEST_BUILD_DIR)/test_page_alloc: HOST_TEST_CPPFLAGS += -iquote arch/aarch64
$(HOST_TEST_BUILD_DIR)/test_page_alloc: \
	kernel/page_allocator.c kernel/page_allocator.h \
	kernel/memmap.c kernel/memmap.h kernel/alloc.h \
	arch/aarch64/handle_exceptions.h \
	lib/fdt.h lib/align.h \
	lib/ccontainers/dynamic_array.h lib/cmemory/freelist.h \
	tests/host/fdt_builder.h

# This static pattern rule applies to each executable in HOST_TEST_BINARIES.
# % selects its matching tests/host/%.c file. filter keeps only .c prerequisites
# in the compiler command, including the per-suite implementation sources above.
$(HOST_TEST_BINARIES): $(HOST_TEST_BUILD_DIR)/%: tests/host/%.c \
		$(ET_DIR)/et.c $(ET_DIR)/et.h $(ET_DIR)/et_host.c
	mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_TEST_CPPFLAGS) $(HOST_TEST_CFLAGS) -I$(ET_DIR) $(filter %.c,$^) -o $@

# -----------------------------------------------------------------------------
# Utilities and generated header dependencies
# -----------------------------------------------------------------------------
dump: $(KERNEL_ELF)
	$(TARGET_OBJDUMP) -D $<

clean:
	rm -rf $(BUILD_DIR) *.elf *.img *.hex *.map

# Read compiler-generated header dependencies. The leading - allows missing
# files on the first build. Replacing .o with .d gives each dependency filename.
-include $(KERNEL_OBJECTS:.o=.d)
-include $(TARGET_TEST_OBJECTS:.o=.d)
