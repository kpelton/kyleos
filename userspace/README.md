# KyleOS userspace layout

The userspace dependencies are consolidated under this directory while keeping
their build boundaries explicit:

```text
userspace/
├── core/       # former kyleos-userspace tree: init, shell, libc-facing tools
└── programs/   # former newlib-progs tree: extra utilities and test programs
```

`core/` is built first against the repository sysroot. The top-level Makefile
then builds the GUI, shell, and kernel-facing programs from this repository and
stages the resulting binaries under `build/userland/`.

`programs/` contains additional command-line tools and regression programs.
It is intentionally a separate Makefile input because its build and install
rules differ from the core userspace tree.

Neither directory should write generated objects into the source tree during a
normal build. Generated libraries, headers, compiler output, staged binaries,
and disk images belong under `build/`.

The complete dependency flow is:

```text
external/newlib + toolchain/newlib-build
                 ↓
             build/sysroot
                 ↓
 userspace/core + userspace/programs + extras/
                 ↓
             build/userland
                 ↓
             build/image
```
