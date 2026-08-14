# KyleOS Quake drive installer

This directory is self-contained. It installs the patched Quake executable,
the verified Quake 1.06 shareware PAK, and—when present on the same mounted
filesystem—the fixed KyleOS kernel. Existing Quake save files are preserved.

## Transfer it

From the development computer:

```bash
scp -r kyleos-quake-installer user@other-computer:~/
```

## Install it on the computer holding the KyleOS drive

First identify the exact KyleOS FAT partition:

```bash
lsblk -o NAME,SIZE,FSTYPE,LABEL,MOUNTPOINTS,MODEL
```

Mount that partition, replacing `/dev/sdX1` only after verifying the device:

```bash
sudo mkdir -p /mnt/kyleos
sudo mount /dev/sdX1 /mnt/kyleos
findmnt /mnt/kyleos
ls /mnt/kyleos
```

The mounted filesystem should contain `bin/nushell` or `sbin/init`. Run:

```bash
cd ~/kyleos-quake-installer
sudo ./install.sh /mnt/kyleos
sudo umount /mnt/kyleos
```

The installer refuses `/`, symbolic-link targets, non-mountpoints, read-only
mounts, and filesystems that do not look like KyleOS. It verifies every payload
checksum before copying. It never deletes `*.sav` files.

## Separate boot filesystem

If the KyleOS root drive does not contain `/boot/kernel.bin`, the installer
leaves the bundled kernels under `payload/boot/`. Update the separate GRUB boot
filesystem with `payload/boot/kernel.bin`. `kernel32.bin` is also included for
the QEMU direct-kernel boot workflow.

After booting KyleOS, start the game with:

```text
quake
```
