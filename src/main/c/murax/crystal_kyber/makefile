PROJ_NAME=crystal_kyber
DEBUG=no
BENCH=no
MULDIV=no
COMPRESSED=no
KAT ?= no
BENCH_ROUNDS ?= 2

ROOT := ../../../../..
MLKEM_NATIVE_DIR := $(ROOT)/external/mlkem-native/mlkem

APP_C_SRCS := $(wildcard src/*.c)
APP_CPP_SRCS := $(wildcard src/*.cpp)
APP_ASM_SRCS := $(wildcard src/*.S)
MLKEM_NATIVE_C_SRCS := $(MLKEM_NATIVE_DIR)/mlkem_native.c

ifeq ($(KAT),yes)
	APP_C_SRCS := $(filter-out src/main.c,$(APP_C_SRCS))
	CFLAGS += -DKAT_MODE=1
else
	APP_C_SRCS := $(filter-out src/main_kat.c,$(APP_C_SRCS))
endif

OBJDIR = build

INC  = -I./src -I$(MLKEM_NATIVE_DIR)
LIBS =
LIBSINC = -L$(OBJDIR)
LDSCRIPT = ./src/linker.ld

RISCV_NAME ?= riscv64-unknown-elf
RISCV_PATH ?= /home/borgescaua/opt/riscv-elf-multilib

MABI=ilp32
MARCH := rv32i_zicsr
ifeq ($(MULDIV),yes)
	MARCH := $(MARCH)m
endif
ifeq ($(COMPRESSED),yes)
	MARCH := $(MARCH)ac
endif

CFLAGS += -march=$(MARCH) -mabi=$(MABI) -DNDEBUG -DBENCH_ROUNDS=$(BENCH_ROUNDS)
CFLAGS += -DMLK_CONFIG_FILE=\"mlkem_native_murax_config.h\"
LDFLAGS += -march=$(MARCH) -mabi=$(MABI)

ifeq ($(DEBUG),yes)
	CFLAGS += -g3 -O0
endif

ifeq ($(DEBUG),no)
	CFLAGS += -g -Os
endif

ifeq ($(BENCH),yes)
	CFLAGS += -fno-inline
endif

RISCV_OBJCOPY = $(RISCV_PATH)/bin/$(RISCV_NAME)-objcopy
RISCV_OBJDUMP = $(RISCV_PATH)/bin/$(RISCV_NAME)-objdump
RISCV_CC=$(RISCV_PATH)/bin/$(RISCV_NAME)-gcc

CFLAGS += -MD -fstrict-volatile-bitfields -fno-strict-aliasing -ffreestanding
LDFLAGS += -nostdlib -lgcc -mcmodel=medany -nostartfiles -ffreestanding -Wl,-Bstatic,-T,$(LDSCRIPT),-Map,$(OBJDIR)/$(PROJ_NAME).map,--print-memory-usage

APP_C_OBJS := $(patsubst src/%.c,$(OBJDIR)/src/%.o,$(APP_C_SRCS))
APP_CPP_OBJS := $(patsubst src/%.cpp,$(OBJDIR)/src/%.o,$(APP_CPP_SRCS))
APP_ASM_OBJS := $(patsubst src/%.S,$(OBJDIR)/src/%.o,$(APP_ASM_SRCS))
MLKEM_NATIVE_C_OBJS := $(patsubst $(MLKEM_NATIVE_DIR)/%.c,$(OBJDIR)/mlkem-native/%.o,$(MLKEM_NATIVE_C_SRCS))

OBJS := $(APP_C_OBJS) $(APP_CPP_OBJS) $(APP_ASM_OBJS) $(MLKEM_NATIVE_C_OBJS)

all: $(OBJDIR)/$(PROJ_NAME).elf $(OBJDIR)/$(PROJ_NAME).hex $(OBJDIR)/$(PROJ_NAME).bin $(OBJDIR)/$(PROJ_NAME).asm $(OBJDIR)/$(PROJ_NAME).v

$(OBJDIR)/%.elf: $(OBJS) | $(OBJDIR)
	$(RISCV_CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBSINC) $(LIBS)

%.hex: %.elf
	$(RISCV_OBJCOPY) -O ihex $^ $@

%.bin: %.elf
	$(RISCV_OBJCOPY) -O binary $^ $@

%.v: %.elf
	$(RISCV_OBJCOPY) -O verilog $^ $@

%.asm: %.elf
	$(RISCV_OBJDUMP) -S -d $^ > $@

$(OBJDIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(RISCV_CC) -c $(CFLAGS) $(INC) -o $@ $^
	$(RISCV_CC) -S $(CFLAGS) $(INC) -o $@.disasm $^

$(OBJDIR)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(RISCV_CC) -c $(CFLAGS) $(INC) -o $@ $^

$(OBJDIR)/%.o: %.S
	mkdir -p $(dir $@)
	$(RISCV_CC) -c $(CFLAGS) -o $@ $^ -D__ASSEMBLY__=1

$(OBJDIR)/mlkem-native/%.o: $(MLKEM_NATIVE_DIR)/%.c
	mkdir -p $(dir $@)
	$(RISCV_CC) -c $(CFLAGS) $(INC) -o $@ $^
	$(RISCV_CC) -S $(CFLAGS) $(INC) -o $@.disasm $^

$(OBJDIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(OBJDIR)/src
	rm -rf $(OBJDIR)/mlkem-native
	rm -f $(OBJDIR)/$(PROJ_NAME).elf
	rm -f $(OBJDIR)/$(PROJ_NAME).hex
	rm -f $(OBJDIR)/$(PROJ_NAME).bin
	rm -f $(OBJDIR)/$(PROJ_NAME).map
	rm -f $(OBJDIR)/$(PROJ_NAME).v
	rm -f $(OBJDIR)/$(PROJ_NAME).asm
	find $(OBJDIR) -type f -name '*.o' -print0 | xargs -0 -r rm
	find $(OBJDIR) -type f -name '*.d' -print0 | xargs -0 -r rm

clean-all : clean

.SECONDARY: $(OBJS)
