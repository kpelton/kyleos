CC ?= gcc
NEWLIB_INSTALL ?= /tmp/z

SOURCES = \
	chase.c cl_demo.c cl_input.c cl_main.c cl_parse.c cl_tent.c \
	cmd.c common.c console.c crc.c cvar.c draw.c \
	d_edge.c d_fill.c d_init.c d_modech.c d_part.c d_polyse.c d_scan.c \
	d_sky.c d_sprite.c d_surf.c d_vars.c d_zpoint.c \
	host.c host_cmd.c keys.c menu.c mathlib.c model.c nonintel.c \
	pr_cmds.c pr_edict.c pr_exec.c \
	r_aclip.c r_alias.c r_bsp.c r_draw.c r_edge.c r_efrag.c r_light.c \
	r_main.c r_misc.c r_part.c r_sky.c r_sprite.c r_surf.c r_vars.c \
	sbar.c screen.c sv_main.c sv_move.c sv_phys.c sv_user.c \
	view.c wad.c world.c zone.c net_loop.c net_main.c net_vcr.c \
	cd_null.c snd_null.c net_none.c \
	sys_kyleos.c vid_kyleos.c in_kyleos.c

OBJECTS = $(SOURCES:.c=.o)

CFLAGS = -m64 -O2 -g -std=gnu89 -fcommon -fno-strict-aliasing \
	-fno-aggressive-loop-optimizations \
	-fno-builtin -ffreestanding -mcmodel=large -mno-red-zone \
	-nostdlib -nostartfiles -nodefaultlibs -static \
	-DKYLEOS -Dstricmp=strcasecmp -I. -I$(NEWLIB_INSTALL)/include \
	-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare \
	-Wno-implicit-function-declaration -Wno-int-conversion \
	-Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
	-Wno-missing-field-initializers -Wno-dangling-else \
	-Wno-misleading-indentation

quake: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) \
		$(NEWLIB_INSTALL)/lib/libm.a $(NEWLIB_INSTALL)/lib/libc.a -o $@

clean:
	rm -f $(OBJECTS) quake

.PHONY: clean
