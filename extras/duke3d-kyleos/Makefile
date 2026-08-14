CC ?= gcc
NEWLIB_INSTALL ?= /tmp/z

ENGINE_DIR := Engine/src
GAME_DIR := Game/src
PORT_DIR := kyleos

ENGINE_SOURCES := \
	cache.c draw.c engine.c filesystem.c fixedPoint_math.c network.c tiles.c \
	dummy_multi.c
GAME_SOURCES := \
	actors.c animlib.c config.c console.c control.c cvar_defs.c cvars.c \
	game.c gamedef.c global.c keyboard.c menues.c player.c premap.c rts.c \
	scriplib.c sector.c sounds.c
PORT_SOURCES := \
	display_kyleos.c audio_kyleos.c dirent_kyleos.c allocator_kyleos.c \
	compat_kyleos.c

SOURCES := $(ENGINE_SOURCES) $(GAME_SOURCES) $(PORT_SOURCES)
OBJECTS := $(SOURCES:.c=.o)

VPATH := $(ENGINE_DIR):$(GAME_DIR):$(PORT_DIR)

CPPFLAGS := \
	-I$(PORT_DIR) -I$(GAME_DIR) -I$(ENGINE_DIR) \
	-I$(NEWLIB_INSTALL)/include \
	-DKYLEOS=1 -DPLATFORM_UNIX=1 -DUSE_SDL=0 \
	-DUSER_DUMMY_NETWORK=1 -D__linux__=1 -DBYTE_ORDER=1234 \
	-include stdint.h \
	-Dmalloc=duke_malloc -Dfree=duke_free \
	-Drealloc=duke_realloc -Dcalloc=duke_calloc

CFLAGS := -m64 -O2 -g -std=gnu99 -fcommon -fno-strict-aliasing \
	-fno-aggressive-loop-optimizations -fno-builtin -ffreestanding \
	-mcmodel=large -mno-red-zone -nostdlib -nostartfiles -nodefaultlibs \
	-static -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare \
	-Wno-pointer-sign -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
	-Wno-parentheses -Wno-maybe-uninitialized \
	-Wno-unused-but-set-variable -Wno-unused-function \
	-Wno-unused-result -Wno-implicit-function-declaration \
	-Wno-int-conversion -Wno-incompatible-pointer-types -Wno-format

duke3d: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) \
		$(NEWLIB_INSTALL)/lib/libm.a $(NEWLIB_INSTALL)/lib/libc.a -o $@

premap.o: $(GAME_DIR)/premap.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -O0 -c $< -o $@

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) duke3d

.PHONY: clean
