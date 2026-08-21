# KyleOS

KyleOS is a hobby x86-64 operating system with a small Unix-like userspace.
It boots directly in QEMU, uses a FAT32 disk image for persistent files, and
has a framebuffer-capable DoomGeneric port.

## Contents

- [What is included](#what-is-included)
- [Kernel and platform](#kernel-and-platform)
- [Desktop and GUI](#desktop-and-gui)
- [Included programs](#included-programs)
- [Technical feature matrix](#technical-feature-matrix)
- [Build and boot](#build-and-boot)
- [Testing and diagnostics](#testing-and-diagnostics)
- [Source layout](#current-source-layout)
- [Limitations](#current-limitations)

## What is included

KyleOS is a bootable system rather than a userspace demo. The image combines a
small preemptive kernel, a FAT32-backed root filesystem, a Unix-like command
environment, and two window-manager shells sharing the same GUI applications.
It runs in QEMU and on compatible x86 hardware with legacy BIOS and PS/2 input.

## Kernel and platform

- x86-64 long mode with 32-bit bootstrap support and a framebuffer console.
- Demand-zero heap and stack paging, lazy file-backed ELF paging, physical-page
  allocation, and a checked first-fit kernel heap.
- Preemptive round-robin scheduling, user processes, `fork`, copy-on-write,
  `exec`, `wait`, file descriptors, pipes, and process reaping.
- FAT32 VFS with directories, file creation/truncation, seek, sizes, removal,
  and separate RAM filesystems at `/dev` and `/tmp`.
- `/dev/console`, `/dev/null`, `/dev/zero`, `/dev/kmsg`, UART/serial input,
  keyboard input, PS/2 mouse input, and raw-console mode.
- User-mode page and general-protection faults terminate the offending process;
  kernel faults retain register-dump diagnostics.

## Desktop and GUI

`/bin/kuid` is the shared compositor and window server. It owns the framebuffer,
input routing, focus, window geometry, display-command validation, and process
lifecycle. Applications communicate through the versioned KUI event/response
protocol over pipes.

- `desktop`: classic taskbar and Start-menu shell.
- `aqua`: Aqua-inspired menu bar, Dock, traffic-light controls, and glass-style
  scrollbars.
- Shared KUI menus, text selection, file dialogs, Save As confirmation, wheel
  scrolling, repeating scrollbar arrows, and window resize handling.
- Applications include Finder, Text Editor, C IDE, Terminal, KSheet, Paint,
  Help, System Monitor, games, and parallel-writer demonstrations.

See the [GUI architecture guide](docs/gui-architecture.html),
[Markdown architecture notes](docs/gui-architecture.md), and the
[Aqua desktop screenshot](docs/kyleos-aqua-desktop.png).

## Included programs

### Core commands

`ls`, `cat`, `cp`, `mv`, `rm`, `mkdir`, `rmdir`, `grep`, `wc`, `head`, `tail`,
`ed`, `kedit`, `sh`, and the filesystem/process regression tools.

### GUI applications

Finder, Text Editor, C IDE, Terminal, KSheet spreadsheet, Paint, Help,
Calculator, System Monitor, Minesweeper, Solitaire, Tetris, City Builder,
Parallel Writers, and the Aqua/Win95 shell demos.

### Optional extras

DoomGeneric, Quake, Duke Nukem 3D, Lua 5.4.4, and the native TinyCC compiler
are built and installed by their respective image targets.

## Technical feature matrix

- x86-64 long mode, demand-zero heap and stack paging, lazy file-backed ELF
  executable paging, physical-memory allocation, and a 16-byte-aligned
  first-fit kernel heap with block splitting, adjacent-block coalescing, and
  corruption checks.
- Preemptive round-robin scheduling with user processes, copy-on-write
  `fork`, demand-paged `exec`, `wait`, file descriptors, and standard
  input/output/error.
- A userspace `/sbin/init` running as PID 1, with orphan adoption, child
  reaping, and automatic console-shell restart.
- FAT32 root filesystem plus separate RAM filesystems at `/dev` and `/tmp`.
- `/dev/console`, `/dev/null`, `/dev/zero`, and a writable 64 KiB kernel-log
  ring at `/dev/kmsg` with monotonic timestamps.
- Directories, file and empty-directory removal, 8.3-safe rename,
  truncate-on-open, seek, file sizes, and a simple VFS.
- Pipes and shell redirection, including chained pipelines, repeat loops,
  `*`/`?` wildcard expansion, and `$?` reporting for the previous foreground
  command.
- In-image filesystem, heap, and user-fault regression suites, plus a single
  integrated runner: `nushell < /tests/all.sh`.
- User-mode page and general-protection faults terminate only the offending
  process; kernel-mode faults retain the register-dump panic path.
- UART/serial input, keyboard input, raw-console mode, and a framebuffer.
- PS/2 mouse input and a shared userspace compositor at `/bin/kuid`. Launch
  `/bin/desktop` for the classic taskbar/Start environment or `/bin/aqua` for
  the Aqua-inspired menu bar, Dock, and traffic-light window controls. Both
  environments run the same KUI application processes and compositor.
- Boots on compatible physical x86 hardware through legacy BIOS, with PS/2
  keyboard and mouse support in addition to QEMU.
- Small userspace: `ls`, `cat`, `cp`, `rm`, `mkdir`, `grep`, `wc`, `head`,
  `tail`, `mv`, `ed`, `kedit`, and more.
- Incrementally rendered ASCII Breakout at `/bin/breakout` (`a`/`d` move,
  `q` quits), with input/render timing independent from ball movement.
- DoomGeneric as an optional extra, installed at `/usr/bin/doom`.
- Original Quake's C software renderer as an optional silent, keyboard-only
  extra, installed at `/usr/bin/quake`.
- Chocolate Duke3D's Build software renderer as an optional silent,
  keyboard-only extra, installed at `/usr/bin/duke3d`.
- Lua 5.4.4 as an optional Newlib-built extra, installed at `/usr/bin/lua`.
- A native x86-64 TinyCC at `/bin/cc`, including headers, static Newlib,
  compiler sources, and a tested stage-2/stage-3 self-bootstrap path.

### KUI desktop environments

`kuid` owns the framebuffer, input routing, application surfaces, focus,
authoritative window geometry, and command validation. It starts one privileged
shell process and exchanges versioned state, drawing commands, hit consumption,
and shell actions over pipes. `win95-shell` owns the classic taskbar and Start
menu; `aqua-shell` owns the top menu bar and Dock. Window decorations use the
metrics and style supplied by that shell.

Applications receive the shell style in their normal KUI event stream, so the
same Finder, editor, terminal, games, dialogs, and menus render consistently in
either environment. Pipe-readiness status and bounded compositor reads prevent
a disconnected or stalled client from blocking the display server forever.

### GUI architecture

The detailed GUI architecture, event/rendering flows, Save As path, and source
map are documented in [the HTML architecture guide](docs/gui-architecture.html)
and [the Markdown version](docs/gui-architecture.md).

![KyleOS Aqua desktop with KSheet and Help running](docs/kyleos-aqua-desktop.png)

This live QEMU capture shows the Aqua composition with KSheet, Help, Finder,
and the Dock running together. The shell owns the menu bar, window placement,
and Dock while applications render through the shared KUI protocol and widget
library. The [architecture preview](docs/gui-aqua-overview.svg) provides a
clean diagrammatic overview as well.

From the KyleOS console, start either environment with:

```sh
desktop
aqua
```

## Prerequisites

On a Debian/Ubuntu-style host, install the host tools:

```bash
sudo apt install build-essential gcc-13 nasm binutils qemu-system-x86 \
  dosfstools kpartx util-linux imagemagick python3-pil fontconfig \
  fonts-dejavu-core
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
├── sbin/
│   └── init             # PID 1 and console supervisor
├── dev/                 # RAM filesystem
│   ├── console
│   ├── null
│   ├── zero
│   └── kmsg
├── tmp/                 # writable RAM filesystem
└── usr/
    ├── bin/             # optional extras
    ├── include/         # Newlib headers for native compilation
    ├── lib/             # static libc, libm, crt0, and libtcc1
    ├── share/
    └── src/             # TinyCC and its KyleOS port overlay
```

`nushell` searches `/bin` and then `/usr/bin` for bare command names.  This
is currently fixed lookup behavior, not a configurable environment `PATH`.
Absolute paths begin at `/`; relative paths begin in the current directory.
The `repeat` builtin runs a command a fixed number of times and supports the
shell's normal input and output redirection:

```sh
repeat 200 ls > /dev/null
```

## Init and userspace boot

The kernel starts `/sbin/init` as PID 1.  Init inherits the three console file
descriptors, forks `/bin/nushell`, waits for it, and restarts it whenever it
exits or is terminated by a userspace fault.  When another process exits, its
live children are reparented to PID 1 so init can reap them.

For compatibility with older disk images, the kernel falls back to
`/bin/nushell` and then the legacy root-level `nushell` when `/sbin/init` is
absent.  Run `make image-reset` once after upgrading to install the real init
binary into an existing generated image.

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

## Quake extra

The original GPL Quake engine is pinned as the `extras/quake` submodule. The
KyleOS port uses the portable C software renderer at 320x200, scaled to the
640x400 framebuffer. Sound, mouse input, and external networking are disabled
for the first port; single-player and loopback games use the keyboard.

Place legally obtained Quake data in an `id1` directory containing at least
`pak0.pak`, then build and recreate the image:

```bash
make quake
QUAKE_DATA_DIR=/path/to/id1 make quake-image
make test
```

For a one-command setup using the freely distributable Quake 1.06 shareware
episode, install an LHA extractor (`sudo apt install lhasa` on Debian/Ubuntu)
and run:

```bash
scripts/setup-quake-shareware.sh
```

The script verifies both the downloaded archive and extracted PAK before
installing it. Pass `--data-only` to install `pak0.pak` without rebuilding the
disk image, or set `QUAKE_SHAREWARE_ARCHIVE=/path/to/quake106.zip` to reuse an
existing download.

When `QUAKE_DATA_DIR` is omitted, the image builder checks
`assets/quake/id1`. It installs available `pak*.pak` files under
`/usr/share/quake/id1`; game data is not part of the source release and is not
tracked here. Run `quake` inside KyleOS. Arrow keys move and turn, Ctrl fires,
Space jumps, Enter selects, and Escape opens or closes the menu.

## Duke Nukem 3D extra

Chocolate Duke3D is pinned as the `extras/duke3d` submodule. The KyleOS port
uses the Build software renderer at 320x200, scaled to the 640x400 framebuffer.
The initial port is single-player, silent, and keyboard-only; mouse, sound,
and external networking are disabled.

Supply a legally obtained shareware, registered 1.3D, or Atomic Edition
`DUKE3D.GRP`, then build and recreate the image:

```bash
make duke3d
DUKE3D_GRP=/path/to/DUKE3D.GRP make duke3d-image
make test
```

When `DUKE3D_GRP` is omitted, the image builder checks
`assets/duke3d/duke3d.grp`. It installs the file as
`/usr/share/duke3d/duke3d.grp`; the proprietary game data is not included in
the source repository. Run `duke3d` inside KyleOS. The default keyboard layout
uses the arrow keys to move and turn, Ctrl to fire, Space to use, Enter to
select, and Escape to open or close the menu.

## Testing and diagnostics

Host-side integration tests cover KUI shell protocol behavior, KSheet and
Editor Save As, terminal responsiveness, Help navigation, text selection,
scrollbar behavior, and widget styles. Run the focused GUI checks with:

```bash
make test-kui-shells test-ksheet-ui test-editor-clipboard \
  test-terminal-async test-help-ui
```

The shell writes app lifecycle and protocol failures to `/SHELL.LOG` without
polluting the KUI stdout stream. In the running system, inspect it with:

```sh
cat /SHELL.LOG
```

The integrated in-OS regression runner is documented below.

After booting, run the bundled suite through the shell:

```sh
nushell < /tests/fs-suite.sh
```

It prints an individual PASS/FAIL line and a final report.  The suite covers
FAT and `/tmp` file operations, directory removal, and `/dev/null` and
`/dev/zero`; it cleans up its own test files.

To exercise repeated copy, byte-integrity, and cleanup paths using the bundled
`/bin/nushell` binary, run:

```sh
nushell < /tests/heap-copy.sh
```

It copies the binary to `/tmp` four times, compares every byte, and verifies
that each temporary file was removed.

The user-fault regression test deliberately executes a privileged instruction
in a child process; it should print `Segfault` and return to its invoking
shell:

```sh
nushell < /tests/gp-fault.sh
```

The demand-paging regression checks lazy file-backed executable pages and
zero-filled BSS, reserves a sparse heap, grows the stack, checks zero-filled
pages and copy-on-write isolation, and then exhausts userspace memory to
verify that the kernel kills only the allocating process:

```sh
nushell < /tests/demand-paging.sh
```

Run every in-image regression in sequence with:

```sh
nushell < /tests/all.sh
```

The integrated runner stops at the first failure and exits nonzero. It prints
`ALL-TESTS-COMPLETE count=9` exactly once only after checking every child exit
status, copy integrity and cleanup, isolated fault status, and all 200 exec
iterations. The copy phase uses `/bin/nushell`, so it has no optional fixture.

Run the host control-flow check and the complete snapshot-based QEMU suite with:

```sh
make test-alltest-runner
make test-suite
```

`make test-suite` rebuilds a dedicated `build/image/test-suite.img`; it does not
modify the normal persistent development image.

## Native C compiler and bootstrap

TinyCC is pinned as the clean `extras/tinycc` submodule. KyleOS-specific
configuration, compatibility functions, CRT, and the upstream patch live in
`extras/tinycc-kyleos`; the build applies that patch only to a disposable copy
under `build/`, so the submodule stays clean.

Build the native compiler and its runtime payload, then recreate the image:

```bash
make bootstrap-cc
make image-reset
```

Compile and run a C program inside KyleOS:

```sh
cc /tests/hello.c -o /tmp/hello
/tmp/hello
```

KyleOS also includes a separate graphical C IDE. After rebuilding the image,
start the Aqua desktop and open **C IDE** from the Dock (it is also available
from the KyleOS 95 Start menu):

```sh
aqua
```

The IDE opens `/hello.c` by default and provides line numbers, C syntax
highlighting, Open/Save controls, and integrated Build and Run actions. Press
F5 to compile the current source with `/bin/cc`, or F6 to compile and run it.
Builds run in the background so the desktop remains responsive. Compiler
diagnostics are shown in the IDE status bar and written to
`/tmp/cide.log`; successful builds produce `/tmp/cide.out`.

The complete self-hosting check builds TinyCC stage 2 with the host-built
KyleOS compiler, builds stage 3 with stage 2, and uses both native stages to
compile and run hello world:

```sh
nushell < /tests/cc-bootstrap.sh
```

Kernel messages can be read repeatedly or injected from userspace:

```sh
cat /dev/kmsg
echo "userspace diagnostic" > /dev/kmsg
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
- Init currently supervises one console shell; it does not yet parse service
  configuration, manage dependencies, or provide shutdown/reboot targets.
- The FAT implementation is intentionally small; rename targets must use
  conventional 8.3-compatible names.
- No hard links, symlinks, full terminal/termios support, or configurable
  shell environment.
- Channel wait queues are implemented for blocking `waitpid` and mutex
  contention; broader wait-queue coverage and cancellation are still pending.
- Copy-on-write currently uses full TLB flushes at process switches and has no
  swap, page eviction, file-backed `mmap`, shared-memory mappings, or a
  copy-on-write page cache. ELF load segments that overlap at page granularity
  are rejected rather than merged.
- The userspace and Newlib source relocation described above is still in
  progress.

## Sleep channels (KyleOS and xv6)

KyleOS follows the xv6 pattern of associating a blocked task with an opaque
channel pointer.  A waiter records its channel and changes to `BLOCKED`; the
matching `wakeup` operation marks it `READY`, and the scheduler resumes it.
`waitpid` uses the parent task as its channel, while mutexes use the mutex
address.

The important difference is lock ordering.  xv6 serializes the sleep/wakeup
transition with its process-table lock.  KyleOS uses `sched_spinlock` for the
same transition and refuses to sleep when that lock is already held, falling
back to `pause` until the critical section ends.  This prevents recursive
scheduler-lock deadlocks during process teardown.  Wake-one is used for
mutexes; wait-channel wakeups scan all matching sleepers.
