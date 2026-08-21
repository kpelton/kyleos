# Building the kernel inside KyleOS

The first self-hosting milestone is producing `kernel.bin` in the guest.  The
host `Makefile` also creates a disk image and launches QEMU; those are separate
steps and are intentionally not part of this bootstrap builder.

Copy the source tree into `/src/kyleos`, install
`selfhost/kernel-build.sh` as `/bin/kernel-build`, and run:

```sh
kernel-build /src/kyleos /tmp/kernel-build
```

The builder compiles the same kernel object list as the top-level `Makefile`,
assembles `init/loader.s`, and links with `linker.ld`.  It writes the result to
`/tmp/kernel-build/kernel.bin`.

The first failed command identifies the next missing native tool.  In a
currently app-only self-hosting image, the expected blockers are usually:

1. `nasm` for `init/loader.s`;
2. `ld` with ELF64 and linker-script support;
3. only afterward, image creation and boot testing.

Do not copy a host-built object file to work around a failure once validating
the bootstrap.  That would hide which part of the toolchain is not yet native.
