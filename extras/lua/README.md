# Lua for KyleOS

This directory vendors the KyleOS/Newlib port of Lua 5.4.4.  `src/` contains
only Lua source and headers; generated objects, archives, and executables are
ignored.  `examples/` contains small scripts installed in the OS image.

Build the KyleOS interpreter from the repository root:

```sh
make lua
```

Set `SYSROOT=/path/to/newlib-install` if Newlib is not installed at `/tmp/z`.
A fresh image installs the result as `/usr/bin/lua` and installs examples in
`/usr/share/lua/`.

Lua is distributed under its upstream MIT license; see `README.upstream`.
