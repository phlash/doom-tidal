# Build a custom bootloader (program @ flash address 0)

ifndef COMPILER
$(error You must define the location of the Xtensa esp32s3 gcc tools, eg: make COMPILER=<path to Xtensa gcc>)
endif
CC=$(COMPILER)/bin/xtensa-esp32s3-elf-gcc

PYTHON=python3

# the badge serial port in USB Jtag/serial mode
PORT=/dev/ttyACM0

# the flash address to write into, this is ota_0 slot (default boot target)
OTA=0x10000

# .text & .data/.bss reside in SRAM (these values are at the lowest SRAM addresses, to avoid bootloader)
# NB: IRAM addresses are /aliases/ of DRAM (+0x6F0000), so you can have overlaps.. beware!
# NB: we explicitly put .rodata in DRAM (R/W) as that's required to support reads other than 32-bit wide :(
TEXTBASE=0x40378000
ROBASE=  0x3FC89000		# 4k (1 page) above TEXTBASE
DATABASE=0x3FC8A000		# 4k (1 page) above RODATA
# start here
ENTRYSYM=entry
# ROM symbol addresses...
ETS_PRINTF=0x400005d0
ETS_DELAYUS=0x40000600

BUILD=build

all: $(BUILD) $(BUILD)/phlashboot.bin

clean:
	rm -rf $(BUILD)

install: all
	$(PYTHON) $(BUILD)/esptool.py --chip esp32s3 --port $(PORT) write_flash $(OTA) $(BUILD)/phlashboot.bin

.PHONY: clean

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/phlashboot.bin: $(BUILD)/phlashboot.elf $(BUILD)/esptool.py
	$(PYTHON) $(BUILD)/esptool.py --chip esp32s3 elf2image --flash_freq 80m --flash_mode dio --flash_size 8MB --output $@ $<
	
$(BUILD)/phlashboot.elf: $(BUILD)/phlashboot.o
	$(CC) \
		-Wl,-gc-sections \
		-nostdlib \
		-Wl,-Ttext=$(TEXTBASE) \
		-Wl,-Tdata=$(DATABASE) \
		-Wl,--section-start=.rodata=$(ROBASE) \
		-Wl,-e,$(ENTRYSYM) \
		-Wl,--defsym=Rets_printf=$(ETS_PRINTF) \
		-Wl,--defsym=Rets_delay_us=$(ETS_DELAYUS) \
		-o $@ $^

$(BUILD)/%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

# This is the appropriate version of esptool from ESP-IDF subproject - honest.
$(BUILD)/esptool.py:
	wget --quiet -O $@ https://github.com/espressif/esptool/raw/b082b0ed2d86b3330134c4854a021dfd14c29b08/esptool.py
