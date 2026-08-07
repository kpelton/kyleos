# KyleOS

KyleOS is a hobby x86-64 operating system with a small Unix-like userspace.
It boots directly in QEMU, uses a FAT32 disk image for persistent files, and
has a framebuffer-capable DoomGeneric port.

## Features

- x86-64 long mode, paging, physical-memory allocation, and a 16-byte-aligned
  first-fit kernel heap with block splitting, adjacent-block coalescing, and
  corruption checks.
- Preemptive round-robin scheduling with user processes, copy-on-write
  `fork`, `exec`, `wait`, file descriptors, and standard input/output/error.
- FAT32 root filesystem plus separate RAM filesystems at `/dev` and `/tmp`.
- `/dev/console`, `/dev/null`, and `/dev/zero`.
- Directories, file and empty-directory removal, 8.3-safe rename,
  truncate-on-open, seek, file sizes, and a simple VFS.
- Pipes and shell redirection, including chained pipelines.
- In-image filesystem, heap, and user-fault regression suites, plus a single
  integrated runner: `nushell < /tests/all.sh`.
- User-mode page and general-protection faults terminate only the offending
  process; kernel-mode faults retain the register-dump panic path.
- UART/serial input, keyboard input, raw-console mode, and a framebuffer.
- Small userspace: `ls`, `cat`, `cp`, `rm`, `mkdir`, `grep`, `wc`, `head`,
  `tail`, `mv`, `ed`, `kedit`, and more.
- Incrementally rendered ASCII Breakout at `/bin/breakout` (`a`/`d` move,
  `q` quits), with input/render timing independent from ball movement.
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

If `DOOM_WAD` is omitted, `doom-image` looks for `assets/doom.wad`.  The image
builder installs the selected file as `/usr/share/doom/doom.wad`; Doom will
start without it but will stop with an `IWAD file ... not found` message.

## In-OS regression tests

After booting, run the bundled suite through the shell:

```sh
nushell < /tests/fs-suite.sh
```

It prints an individual PASS/FAIL line and a final report.  The suite covers
FAT and `/tmp` file operations, directory removal, and `/dev/null` and
`/dev/zero`; it cleans up its own test files.

To stress the kernel heap with repeated large RAMFS allocations, after the
Bible text has been installed run:

```sh
nushell < /tests/heap-copy.sh
```

It copies `/usr/share/text/bible.txt` to `/tmp` four times, removing each
temporary copy before the next pass.

The user-fault regression test deliberately executes a privileged instruction
in a child process; it should print `Segfault` and return to its invoking
shell:

```sh
nushell < /tests/gp-fault.sh
```

Run every in-image regression in sequence with:

```sh
nushell < /tests/all.sh
```

The integrated runner ends with `ALL-TESTS-COMPLETE`.  It requires the Bible
text installed by `make image` or `make image-reset` for the heap-copy phase.

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

The top-level build tracks the Lua source timestamps, so ordinary `make test`
runs reuse the existing Lua binary unless its sources changed.

`life.lua` accepts an optional generation count, for example
`lua /usr/share/lua/life.lua 50`.

The image also includes serial-friendly terminal games: `rogue.lua`,
`snake.lua`, `hunt.lua`, `lander.lua`, `invaders.lua`, `empire.lua`,
`maze.lua`, and `shquest.lua`. They use line commands, so they work over the
serial console without raw terminal support.

## Searchable text corpus

If `assets/bible.txt` is present, image creation installs the public-domain
Project Gutenberg King James Bible at `/usr/share/text/bible.txt` (about 4.4
MiB).  Search it inside KyleOS, for example:

```sh
grep "In the beginning" /usr/share/text/bible.txt
grep "Moses" /usr/share/text/bible.txt | grep "said"
```

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
