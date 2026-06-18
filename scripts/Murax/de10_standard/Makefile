ROOT := ../../..
FIRMWARE_DIR := $(ROOT)/src/main/c/murax/crystal_kyber
QUARTUS_SH ?= quartus_sh
QUARTUS_PGM ?= quartus_pgm
RISCV_PATH ?=
RISCV_NAME ?= riscv64-unknown-elf
BENCH_ROUNDS ?= 1
CABLE ?= 1
HPS_DEVICE ?= SOCVHPS
FPGA_DEVICE_INDEX ?= 2
PROJECT := MuraxDe10Standard
SOF := output_files/$(PROJECT).sof

.PHONY: all build firmware kat-firmware rtl compile program kat kat-program clean

all: build

build:
	$(MAKE) firmware
	$(MAKE) rtl
	$(MAKE) compile

firmware:
	$(MAKE) -B -C $(FIRMWARE_DIR) RISCV_PATH=$(RISCV_PATH) RISCV_NAME=$(RISCV_NAME) BENCH_ROUNDS=$(BENCH_ROUNDS) FPGA_LED_ONLY=yes all

kat-firmware:
	python3 $(ROOT)/scripts/generate_mlkem512_kat_vectors.py
	$(MAKE) -B -C $(FIRMWARE_DIR) RISCV_PATH=$(RISCV_PATH) RISCV_NAME=$(RISCV_NAME) KAT=yes FPGA_LED_ONLY=yes all

rtl:
	cd $(ROOT) && sbt "runMain vexriscv.demo.MuraxCrystalKyberDe10Standard"

compile:
	$(QUARTUS_SH) --flow compile $(PROJECT)

program: $(SOF)
	$(QUARTUS_PGM) -c "$(CABLE)" -m jtag -o "s;$(HPS_DEVICE)@1" -o "p;$(SOF)@$(FPGA_DEVICE_INDEX)"

kat:
	$(MAKE) kat-firmware
	$(MAKE) rtl
	$(MAKE) compile

kat-program:
	$(MAKE) kat
	$(MAKE) program

$(SOF):
	@echo "Missing $(SOF); run: make build"
	@exit 1

clean:
	rm -rf db incremental_db output_files *.rpt *.summary *.smsg *.qws *.jdi *.pin *.sof
