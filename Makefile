# Let's build a native micropython module containing idTech1 (doom) engine..

#ifndef $(COMPILER)
#$(error You must define the location of the Xtensa esp32s3 cross compiler, eg: make COMPILER=/home/tools/xtensa-esp32s3-elf-gcc)
#endif

DOOMSRC = dummy.c am_map.c doomdef.c doomstat.c dstrings.c d_event.c d_items.c d_iwad.c d_loop.c d_main.c d_mode.c d_net.c f_finale.c f_wipe.c g_game.c hu_lib.c hu_stuff.c info.c i_cdmus.c i_endoom.c i_joystick.c i_scale.c i_sound.c i_system.c i_timer.c memio.c m_argv.c m_bbox.c m_cheat.c m_config.c m_controls.c m_fixed.c m_menu.c m_misc.c m_random.c p_ceilng.c p_doors.c p_enemy.c p_floor.c p_inter.c p_lights.c p_map.c p_maputl.c p_mobj.c p_plats.c p_pspr.c p_saveg.c p_setup.c p_sight.c p_spec.c p_switch.c p_telept.c p_tick.c p_user.c r_bsp.c r_data.c r_draw.c r_main.c r_plane.c r_segs.c r_sky.c r_things.c sha1.c sounds.c statdump.c st_lib.c st_stuff.c s_sound.c tables.c v_video.c wi_stuff.c w_checksum.c w_file.c w_main.c w_wad.c z_zone.c w_file_stdc.c i_input.c i_video.c doomgeneric.c

MPY_DIR=./micropython
MOD = doomgeneric
SRC = doomwrapper.c $(addprefix doomgeneric/doomgeneric/,$(DOOMSRC))
ARCH = xtensawin
CFLAGS_EXTRA = -D_DEFAULT_SOURCE -Idoomgeneric/doomgeneric
# support for additional objects to link, patched into micropython
OBJ_EXTRA = _divsf3.o _extendsfdf2.o _truncdfsf2.o _floatsidf.o _divdf3.o _muldf3.o _addsubdf3.o _divdi3.o _cmpdf2.o
LINK_EXTRA = $(addprefix $(BUILD)/,$(OBJ_EXTRA))

include $(MPY_DIR)/py/dynruntime.mk
CROSS = $(COMPILER)/bin/xtensa-esp32s3-elf-

#MPY_LD += '-vvv'

PATCH_FLAG = $(BUILD)/patch.applied

$(CONFIG_H): $(PATCH_FLAG)

$(PATCH_FLAG): doomgeneric.patch micropython.patch
	cd doomgeneric; \
	git apply ../doomgeneric.patch; \
	cd ../micropython; \
	git apply ../micropython.patch; \
	cd ..; \
	touch $@

# Extract specific additional objects from libgcc for linking, because mpy_ld.py is too dumb :-(
$(LINK_EXTRA): $(COMPILER)/lib/gcc/xtensa-esp32s3-elf/11.2.0/libgcc.a
	$(CROSS)ar x $< --output $(BUILD) $(notdir $@)

# Magical munging to clean patching
CLEAN_EXTRA = doomgeneric.mpy; cd doomgeneric; git reset --hard; cd ../micropython; git reset --hard
