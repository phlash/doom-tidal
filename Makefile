# Let's build a native micropython module containing a loader (this will mess with MMU tables..)
# plus the iDTech1 engine (aka DOOM) built as an ELF, munged into an ESP format loadable image.

ifndef COMPILER
$(error You must define the location of the Xtensa esp32s3 cross compiler, eg: make COMPILER=/home/tools/xtensa-esp32s3-elf-gcc)
endif

# Set build folder
BUILD = build

# Define first Makefile target to build both the loader and the engine
stuff: all $(BUILD) $(BUILD)/doom.bin
.PHONY: stuff

$(BUILD):
	mkdir -p $@

# Target device access
TTY=/dev/ttyACM0
APP=Doom
RESET=--no-soft-reset

MPY_DIR=./micropython
MOD = doomloader
SRC = doomloader.c
ARCH = xtensawin
CFLAGS_EXTRA = -Idoomgeneric/doomgeneric
# support for additional objects to link, patched into micropython
#OBJ_EXTRA = _divsf3.o _extendsfdf2.o _truncdfsf2.o _floatsidf.o _divdf3.o _muldf3.o _addsubdf3.o _divdi3.o _cmpdf2.o
#LINK_EXTRA = $(addprefix $(BUILD)/,$(OBJ_EXTRA))

include $(MPY_DIR)/py/dynruntime.mk
CROSS = $(COMPILER)/bin/xtensa-esp32s3-elf-


PATCH_FLAG = $(BUILD)/patch.applied

$(CONFIG_H): $(PATCH_FLAG)

$(PATCH_FLAG): doomgeneric.patch micropython.patch
	cd doomgeneric; \
	git apply ../doomgeneric.patch; \
	cd ../micropython; \
	git apply ../micropython.patch; \
	cd ..; \
	touch $@

# Magical munging to clean patching
CLEAN_EXTRA = $(MOD).mpy; cd doomgeneric; git reset --hard; cd ../micropython; git reset --hard

# TiDAL badge install
install: stuff __init__.py
	$(MPY_DIR)/tools/pyboard.py -d $(TTY) $(RESET) cleanup.py
	$(MPY_DIR)/tools/pyboard.py -d $(TTY) $(RESET) -f mkdir /apps/$(APP)
	$(MPY_DIR)/tools/pyboard.py -d $(TTY) $(RESET) -f cp __init__.py :/apps/$(APP)/
	$(MPY_DIR)/tools/pyboard.py -d $(TTY) $(RESET) -f cp doomgeneric.mpy :/apps/$(APP)/

# iDTech1 (DOOM) engine build
DOOMSRC = dummy.c am_map.c doomdef.c doomstat.c dstrings.c d_event.c d_items.c d_iwad.c d_loop.c d_main.c d_mode.c d_net.c f_finale.c f_wipe.c g_game.c hu_lib.c hu_stuff.c info.c i_cdmus.c i_endoom.c i_joystick.c i_scale.c i_sound.c i_system.c i_timer.c memio.c m_argv.c m_bbox.c m_cheat.c m_config.c m_controls.c m_fixed.c m_menu.c m_misc.c m_random.c p_ceilng.c p_doors.c p_enemy.c p_floor.c p_inter.c p_lights.c p_map.c p_maputl.c p_mobj.c p_plats.c p_pspr.c p_saveg.c p_setup.c p_sight.c p_spec.c p_switch.c p_telept.c p_tick.c p_user.c r_bsp.c r_data.c r_draw.c r_main.c r_plane.c r_segs.c r_sky.c r_things.c sha1.c sounds.c statdump.c st_lib.c st_stuff.c s_sound.c tables.c v_video.c wi_stuff.c w_checksum.c w_file.c w_main.c w_wad.c z_zone.c w_file_stdc.c i_input.c i_video.c doomgeneric.c
DOOMOBJ = $(addprefix $(BUILD)/,$(DOOMSRC:.c=.o) doomwrapper.o)

# @see memstuff/uPython.map for reasoning behind these values...
TEXTBASE = 0x42200000
DATABASE = 0x3D900000
RODATABASE=0x3C000000

LDFLAGS = -Wl,-e,run_doom \
		  -Wl,--section-start=.text=$(TEXTBASE) \
		  -Wl,--section-start=.rodata=$(RODATABASE) \
		  -Wl,--section-start=.data=$(DATABASE)

$(BUILD)/doom.bin: $(BUILD)/doom $(BUILD)/esptool.py
	python3 $(BUILD)/esptool.py --chip esp32s3 elf2image --flash_freq 80m --flash_mode dio --flash_size 8MB --output $@ $<

$(BUILD)/esptool.py:
	wget --quiet -O $@ https://github.com/espressif/esptool/raw/b082b0ed2d86b3330134c4854a021dfd14c29b08/esptool.py

$(BUILD)/doom: $(DOOMOBJ)
	$(CROSS)gcc -o $@ $(LDFLAGS) $^ $(LIBS)

# replace complex uPython flags..
$(BUILD)/doom: CFLAGS = -D_DEFAULT_SOURCE -I. -Idoomgeneric/doomgeneric
# link only with libgcc for static math functions
$(BUILD)/doom: LIBS = -nostdlib -lgcc

$(BUILD)/%.o: doomgeneric/doomgeneric/%.c
	$(CROSS)gcc -c $(CFLAGS) -o $@ $<

$(BUILD)/doomwrapper.o: doomwrapper.c
	$(CROSS)gcc -c $(CFLAGS) -DMICROPY_ENABLE_DYNRUNTIME -DMP_CONFIGFILE='<$(CONFIG_H)>' -DNO_QSTR -I$(MPY_DIR) -o $@ $<
