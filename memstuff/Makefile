ifndef COMPILER
$(error You must define the location of the Xtensa esp32s3 cross compiler, eg: make COMPILER=/home/tools/xtensa-esp32s3-elf-gcc)
endif

# Target device access
TTY=/dev/ttyACM0
APP=RomRead
RESET=--no-soft-reset

MPY_DIR=../micropython
MOD = romread
SRC = romread.c
ARCH = xtensawin

include $(MPY_DIR)/py/dynruntime.mk
CROSS = $(COMPILER)/bin/xtensa-esp32s3-elf-

# TiDAL badge install
install: $(MOD).mpy romread.py
	$(MPY_DIR)/tools/pyboard.py -d $(TTY) $(RESET) romclean.py
	$(MPY_DIR)/tools/pyboard.py -d $(TTY) $(RESET) -f mkdir /apps/$(APP)
	$(MPY_DIR)/tools/pyboard.py -d $(TTY) $(RESET) -f cp romread.py :/apps/$(APP)/__init__.py
	$(MPY_DIR)/tools/pyboard.py -d $(TTY) $(RESET) -f cp romread.mpy :/apps/$(APP)/

# Local tools:
all: binsegments

binsegments: binsegments.c
	gcc -o $@ $<
