#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_dir=${NEWLIB_SOURCE:-"$root/external/newlib"}
source_dir=$(CDPATH= cd -- "$source_dir" && pwd)
build_dir=${NEWLIB_BUILD:-"$root/build/toolchain/newlib"}
sysroot=${SYSROOT:-"$root/build/sysroot"}

# Newlib's generated configure script expects these Automake support files in
# the directory above its source tree.  They are host build helpers, not
# project source, so materialize them there when a checkout does not already
# have them.  Keeping this setup here makes a fresh clone independent of any
# generated files from a different machine.
source_parent=$(dirname "$source_dir")
automake_libdir=${AUTOMAKE_LIBDIR:-}
if [[ -z "$automake_libdir" ]] && command -v automake >/dev/null 2>&1; then
    automake_libdir=$(automake --print-libdir 2>/dev/null || true)
fi
for helper in config.guess config.sub depcomp; do
    if [[ ! -f "$source_parent/$helper" ]]; then
        if [[ -z "$automake_libdir" || ! -f "$automake_libdir/$helper" ]]; then
            echo "missing Newlib build helper: $helper" >&2
            echo "install Automake or set AUTOMAKE_LIBDIR to its support directory" >&2
            exit 1
        fi
        cp "$automake_libdir/$helper" "$source_parent/$helper"
    fi
done

if [[ ! -f "$build_dir/Makefile" ]]; then
    mkdir -p "$build_dir"
    (
        cd "$build_dir"
        CC="${CC:-gcc}" AS="${AS:-nasm}" \
            CFLAGS="-O2 -mno-mmx -D_LDBL_EQ_DBL -Wno-error -Wno-implicit-function-declaration" \
            "$source_dir/configure" --build=kyleos --prefix="$sysroot" \
                --disable-dependency-tracking
    )
fi

# This KyleOS-era Newlib Makefile combines libm into libc.a but names the
# objects as libc/libm_a-*.o.  Newer GNU make/configure output leaves them in
# libm/*, so materialize the expected aliases before the archive rule runs.
make -C "$build_dir" prefix="$sysroot" exec_prefix="$sysroot" \
    CPPFLAGS="-DSIGNAL_PROVIDED -I$build_dir/targ-include -I$source_dir/libc/include" \
    CFLAGS="-O2 -mno-mmx -D_LDBL_EQ_DBL -DSIGNAL_PROVIDED -Wno-error -Wno-implicit-function-declaration" \
    CCASFLAGS="-O2 -mno-mmx" \
    am__disable_dependency_tracking=yes libm.a
while IFS= read -r object; do
    name=${object##*/}
    mkdir -p "$build_dir/libc"
    ln -sf "$object" "$build_dir/libc/libm_$name"
done < <(find "$build_dir/libm" -type f -name 'a-*.o')
while IFS= read -r object; do
    name=${object##*/}
    target="$build_dir/libc/$name"
    [[ -e "$target" ]] || ln -sf "$object" "$target"
done < <(find "$build_dir/libc" -mindepth 2 -type f -name 'a-*.o')

# Newlib's x86_64 floating-point ABI returns doubles in SSE registers.  Do not
# inherit the kernel's freestanding -mno-sse flags through exported CFLAGS.
make -C "$build_dir" prefix="$sysroot" exec_prefix="$sysroot" \
    CPPFLAGS="-DSIGNAL_PROVIDED -I$build_dir/targ-include -I$source_dir/libc/include" \
    CFLAGS="-O2 -mno-mmx -D_LDBL_EQ_DBL -DSIGNAL_PROVIDED -Wno-error -Wno-implicit-function-declaration" \
    CCASFLAGS="-O2 -mno-mmx" \
    am__disable_dependency_tracking=yes all
make -C "$build_dir" prefix="$sysroot" exec_prefix="$sysroot" install
