VERILOG_SOURCES := $(wildcard *.v) $(wildcard *.sv)
TEST_SOURCES := $(wildcard tb/*.v) $(wildcard tb/*.sv)
OUTPUT := bitstream
PACKAGE := CABGA256
FPGA_SIZE ?= 25k

all: debug

%.json: $(VERILOG_SOURCES)
	rm -f $(OUTPUT).bit $(OUTPUT).config $(OUTPUT).json
	yosys -p 'synth_ecp5 -top top -json $@' $^

LPF ?= icepi-zero.lpf

%.config: %.json $(LPF)
	nextpnr-ecp5 --$(FPGA_SIZE) --package $(PACKAGE) --lpf $(LPF) --json $< --textcfg $@

%.bit: %.config
	ecppack --compress $< $@

build: $(OUTPUT).bit

debug: build
	openFPGALoader -b icepi-zero $(OUTPUT).bit

install: build
	openFPGALoader -b icepi-zero $(OUTPUT).bit --write-flash

install-bitstream:
	openFPGALoader -b icepi-zero $(OUTPUT).bit --write-flash

lint:
	verilator --lint-only -Wall -Wno-DECLFILENAME -Wno-WIDTHEXPAND $(VERILOG_SOURCES)

help:
	echo "Usage: make [option]"
	echo "Options:"
	echo "- install: install to flash (persists across power loss)"
	echo "- debug: load to chip's temp memory (bitstream lost on power loss)"
	echo "- build: builds the bitstream"
	echo "- clean: delete all temporary files"

clean:
	rm -f $(OUTPUT).bit $(OUTPUT).config $(OUTPUT).json

.PHONY: build clean install
