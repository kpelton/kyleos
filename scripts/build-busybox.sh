#!/usr/bin/env bash
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
busybox_source=${BUSYBOX_SOURCE:-"$root/extras/busybox"}
port_dir="$root/extras/busybox-kyleos"
source_dir=${BUSYBOX_WORK_SOURCE:-"$root/build/extras/busybox-src"}
build_dir=${BUSYBOX_BUILD:-"$root/build/extras/busybox-kyleos-build"}
sysroot=${SYSROOT:-"$root/build/sysroot"}
output=${BUSYBOX_BINARY:-"$root/build/userland/busybox"}
host_cc=${HOST_CC:-gcc}
host_ar=${HOST_AR:-ar}
host_nm=${HOST_NM:-nm}
host_strip=${HOST_STRIP:-strip}
expected_commit=fc71374dfccd46448c62947269a35f1420d7ee28

# The parent kernel build exports target flags and command-line overrides.
# BusyBox has its own isolated output tree and configuration, so neither may
# leak into its nested make invocations.
unset CFLAGS CPPFLAGS CXXFLAGS LDFLAGS
unset MAKEFLAGS MFLAGS MAKEOVERRIDES MAKELEVEL

[[ -d $busybox_source/.git || -f $busybox_source/.git ]] || {
    echo "missing BusyBox submodule: run 'git submodule update --init extras/busybox'" >&2
    exit 1
}
[[ $($host_cc -dumpmachine) == x86_64-* ]] || {
    echo "BusyBox currently requires an x86-64 host compiler" >&2
    exit 1
}
[[ -f $sysroot/lib/libc.a ]] || {
    echo "missing KyleOS Newlib: run 'bash scripts/build-newlib.sh' first" >&2
    exit 1
}

actual_commit=$(git -C "$busybox_source" rev-parse HEAD)
[[ $actual_commit == "$expected_commit" ]] || {
    echo "BusyBox must be at 1.38.0 commit $expected_commit (found $actual_commit)" >&2
    exit 1
}

case "$source_dir:$build_dir" in
    "$root"/build/extras/*:"$root"/build/extras/*) ;;
    *) echo "refusing unsafe BusyBox work directories" >&2; exit 1;;
esac
rm -rf -- "$source_dir" "$build_dir"
mkdir -p "$source_dir" "$build_dir" "$(dirname -- "$output")"
git -C "$busybox_source" archive HEAD | tar -x -C "$source_dir"
patch -d "$source_dir" -p1 < "$port_dir/busybox-kyleos.patch"

make -C "$source_dir" O="$build_dir" allnoconfig >/dev/null

# BusyBox's allnoconfig is the reliable way to start from a minimal build.
# Overlay the small KyleOS selection, then let Kconfig resolve dependencies.
while IFS= read -r entry; do
    case "$entry" in
        CONFIG_*=*) option=${entry%%=*} ;;
        *) continue ;;
    esac
    sed -i \
        -e "s|^# $option is not set$|$entry|" \
        -e "s|^$option=.*$|$entry|" \
        "$build_dir/.config"
done < "$port_dir/defconfig"
make -C "$source_dir" O="$build_dir" silentoldconfig >/dev/null

gcc_builtin_include=$($host_cc -print-file-name=include)
target_cflags=(
    -m64
    -nostdinc
    -isystem "$gcc_builtin_include"
    -I"$port_dir/include"
    -isystem "$sysroot/include"
    -ffreestanding
    -fno-stack-protector
    -fno-pie
    -mcmodel=large
    -mno-red-zone
    -U__linux__
    -Ulinux
    -DKYLEOS
)

"$host_cc" "${target_cflags[@]}" \
    -c "$port_dir/compat.c" -o "$build_dir/compat.o"

regex_objects=()
for source in fnmatch regcomp regexec regerror regfree; do
    object="$build_dir/$source.o"
    "$host_cc" "${target_cflags[@]}" \
        -I"$root/external/newlib/libc/posix" \
        -c "$root/external/newlib/libc/posix/$source.c" -o "$object"
    regex_objects+=("$object")
done

"$host_ar" rcs "$build_dir/libbusybox-kyleos.a" \
    "$build_dir/compat.o" "${regex_objects[@]}"

extra_cflags=$(printf '%q ' "${target_cflags[@]}")
extra_ldflags="-nostdlib -nostartfiles -nodefaultlibs -static -no-pie"
extra_ldflags+=" -Wl,-u,main -Wl,--gc-sections"
extra_ldflags+=" -L$build_dir -L$sysroot/lib"

make -C "$source_dir" O="$build_dir" -j"${JOBS:-4}" \
    CC="$host_cc" AR="$host_ar" NM="$host_nm" STRIP="$host_strip" \
    EXTRA_CFLAGS="$extra_cflags" EXTRA_LDFLAGS="$extra_ldflags" busybox

cp "$build_dir/busybox" "$output"
chmod 0755 "$output"
echo "built KyleOS BusyBox: $output"
