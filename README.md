# KyleOS

Toy x86 operating system based on a Unix without multiuser support.

## Build and boot

The generated kernel, userland staging area, mount point, and disk image live
under `build/` and are intentionally not tracked.

```bash
make userland       # builds and stages the boot-image tools
make image          # creates build/image/test-hd.img if it is absent
make test           # builds the kernel and boots the existing image
```

`make image-reset` recreates the image and destroys files stored in it.  Use it
only when that is intended.  Normal `make test` preserves the existing image.
Generated userland programs are installed in `/bin`; optional extras are
installed in `/usr/bin`.

For manual asset changes:

```bash
make image-mount
make image-copy FILE=/path/to/program-or-asset
make image-unmount
```

Copy `config.mk.example` to `config.mk` to override legacy source paths during
the repository migration.  `config.mk` is local and ignored by Git.

## Doom extra

DoomGeneric is tracked as the `extras/doom` submodule.  The KyleOS backend and
its build patch are kept in this repository, so the submodule remains pinned
to a clean upstream revision.

```bash
make doom
DOOM_WAD=/path/to/doom.wad make doom-image
```

`doom-image` recreates the generated image, installs the executable at
`/usr/bin/doom`, and installs the supplied WAD at `/usr/share/doom/doom.wad`.
The WAD is user-supplied and is never tracked by Git.
