# KyleOS

KyleOS is a hobby x86-64 operating system with a small Unix-like userspace.
It boots directly in QEMU, uses a FAT32 disk image for persistent files, and
has a framebuffer-capable DoomGeneric port.

## Features

- x86-64 long mode, paging, physical-memory allocation, and a kernel heap.
- Preemptive round-robin scheduling with user processes, copy-on-write
  `fork`, `exec`, `wait`, file descriptors, and standard input/output/error.
- FAT32 root filesystem plus separate RAM filesystems at `/dev` and `/tmp`.
- `/dev/console`, `/dev/null`, and `/dev/zero`.
- Directories, file creation/removal, 8.3-safe rename, truncate-on-open, seek,
  file sizes, and a simple VFS.
- Pipes and shell redirection, including chained pipelines.
- UART/serial input, keyboard input, raw-console mode, and a framebuffer.
- Small userspace: `ls`, `cat`, `cp`, `rm`, `mkdir`, `grep`, `wc`, `head`,
  `tail`, `mv`, `ed`, `kedit`, and more.
- DoomGeneric as an optional extra, installed at `/usr/bin/doom`.
- Lua 5.4.4 as an optional Newlib-built extra, installed at `/usr/bin/lua`.

## Prerequisites

On a Debian/Ubuntu-style host, install the host tools:

```bash
sudo apt install build-essential gcc-13 nasm binutils qemu-system-x86 \
  dosfstools kpartx util-linux
```

You also need `sudo` access for creating, mapping, formatting, and mounting
the generated FAT image.

Initialize the tracked extras after cloning:

```bash
git submodule update --init --recursive
```

## Current source layout

The kernel and reproducible image tooling live in this repository.  During
the ongoing migration, Newlib and the two userspace source trees are still
configured through local paths.  Copy the example configuration and adjust it
if your layout differs:

```bash
cp config.mk.example config.mk
```

The default configuration expects these existing trees relative to this repo:

```text
../../newlib-build
../../kyleos-userspace
../../newlib-progs
```

`config.mk` is ignored by Git.  The long-term layout is to move these trees
under this repository and use the pinned Newlib fork/submodule.

## Build and boot

Generated output is kept under `build/` and is not tracked.

```bash
make toolchain      # rebuild/install Newlib into the configured sysroot
make userland       # build and stage the base /bin programs
make image          # create build/image/test-hd.img if it is absent
make test           # build the kernel and boot the existing image
```

Use `make image-reset` to intentionally recreate the image.  It destroys all
files stored in that generated image, including savegames and test files.
Normal `make test` preserves the existing image.

The image scripts clean up kpartx mappings on failure.  If an image creation
was interrupted, rerun `make image`; incomplete images are recreated rather
than treated as valid.

## Filesystem layout

New generated images use this layout:

```text
/
├── bin/                 # standard KyleOS tools and the boot shell
│   ├── nushell
│   ├── ls
│   ├── grep
│   └── ...
├── dev/                 # RAM filesystem
│   ├── console
│   ├── null
│   └── zero
├── tmp/                 # writable RAM filesystem
└── usr/
    ├── bin/             # optional extras
    └── share/
```

`nushell` searches `/bin` and then `/usr/bin` for bare command names.  This
is currently fixed lookup behavior, not a configurable environment `PATH`.
Absolute paths begin at `/`; relative paths begin in the current directory.

## Managing the generated image

Mount it when you need to inspect or add a file manually:

```bash
make image-mount
make image-copy FILE=/path/to/program-or-asset
make image-unmount
```

The mount point is `build/image/mnt`.  Do not mount or edit the image while
QEMU is running.

## Doom extra

DoomGeneric is tracked as the `extras/doom` submodule.  The KyleOS platform
backend and build patch remain in this repository, while DoomGeneric itself
stays pinned to a clean upstream revision.  The build happens in `build/`, so
building Doom does not dirty the submodule.

```bash
make doom
DOOM_WAD=/path/to/doom.wad make doom-image
make test
```

`doom-image` recreates the generated image, installs Doom at `/usr/bin/doom`,
and installs the supplied WAD as `/usr/share/doom/doom.wad`.  The WAD is
user-supplied, ignored by Git, and never distributed with KyleOS.  Inside
KyleOS, run:

```sh
doom
```

## Lua extra

Lua 5.4.4 is vendored under `extras/lua` and built against Newlib. A fresh
image installs the interpreter at `/usr/bin/lua` and its bundled examples at
`/usr/share/lua/`:

```sh
lua /usr/share/lua/life.lua
lua /usr/share/lua/adven.lua
```

`life.lua` accepts an optional generation count, for example
`lua /usr/share/lua/life.lua 50`.

The image also includes serial-friendly terminal games: `rogue.lua`,
`snake.lua`, `hunt.lua`, `lander.lua`, `invaders.lua`, `empire.lua`,
`maze.lua`, and `shquest.lua`. They use line commands, so they work over the
serial console without raw terminal support.

## Current limitations

- No multiuser model, permissions, signals, networking, or dynamic linker.
- The FAT implementation is intentionally small; rename targets must use
  conventional 8.3-compatible names.
- No hard links, symlinks, full terminal/termios support, or configurable
  shell environment.
- The scheduler wait path is polling-based; it is tuned for responsiveness,
  not yet a full wait-queue implementation.
- Copy-on-write currently uses full TLB flushes at process switches and has no
  demand paging, swap, shared-memory mappings, or copy-on-write page cache.
- The userspace and Newlib source relocation described above is still in
  progress.
