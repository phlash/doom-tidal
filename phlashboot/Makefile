# Build a custom bootloader (program @ flash address 0)

ifndef COMPILER
$(error You must define the location of the Xtensa esp32s3 gcc tools, eg: make COMPILER=<path to Xtensa gcc>)
endif
CC=$(COMPILER)/bin/xtensa-esp32s3-elf-gcc

# .text & .data/.bss reside in SRAM (these values are used by ESP-IDF 2nd stage loader)
# we explicitly put .rodata in RW memory as that's required to support reads other than 32-bit wide :(
TEXTBASE=0x403B0000
ROBASE=  0x3FC90000
DATABASE=0x3FCD0000
# start here
ENTRYSYM=entry
# ROM symbol addresses...
ETS_PRINTF=0x400005d0
ETS_DELAYUS=0x40000600

BUILD=build

all: $(BUILD) $(BUILD)/phlashboot.bin

clean:
	rm -rf $(BUILD)

.PHONY: clean

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/phlashboot.bin: $(BUILD)/phlashboot.elf $(BUILD)/esptool.py
	python3 $(BUILD)/esptool.py --chip esp32s3 elf2image --flash_freq 80m --flash_mode dio --flash_size 8MB --output $@ $<
	
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
