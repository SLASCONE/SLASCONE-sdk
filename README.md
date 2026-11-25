# SlasconeClient

A C++17 shared library with a C ABI. `slascone_client_abi_version()` returns
`1`. The library verifies signed SLASCONE offline licenses with inline or
separate activation. Offline verification does not contact the service.

For AI-assisted development, see the [agent guide](./AGENTS.md) for repository
conventions, compatibility rules, and verification expectations.

For the public API and integration details, see the [developer reference](./doc/README.md).

## Build dependencies

Outside the dev container, install [vcpkg](https://github.com/microsoft/vcpkg)
and set `VCPKG_ROOT` to its checkout directory before configuring. For a
reproducible setup, check out the `builtin-baseline` commit in
[vcpkg.json](./vcpkg.json) and run
`bootstrap-vcpkg.bat` on Windows or `./bootstrap-vcpkg.sh` on Linux from that
checkout. The manifest installs xmlsec with OpenSSL and libxml2.
Use a fresh build directory when switching an existing build to vcpkg.

## Windows (x86-64 and ARM64)

Install CMake 3.21 or later and Visual Studio 2022 with the Desktop
development with C++ workload. Install the MSVC ARM64 build tools if
targeting ARM64. The presets use `x64-windows-static` and
`arm64-windows-static`, respectively.

```powershell
cmake --preset windows-x64
cmake --build build\windows-x64 --config Release
ctest --test-dir build\windows-x64 -C Release --output-on-failure
```

The DLL is `build/windows-x64/Release/SlasconeClient.dll`. For ARM64,
replace `windows-x64` with `windows-arm64` in the commands and run the
ARM64 smoke test on a Windows ARM64 host.

## Linux (x86-64 and ARM64)

Use CMake 3.21 or later, Ninja, and GCC on the target architecture, with
`pkg-config`, Perl, Linux kernel headers, `zip`, and `unzip` for vcpkg.
In VS Code, **Dev Containers: Reopen in Container** provides these tools,
bootstraps vcpkg at the manifest's `builtin-baseline`, and sets `VCPKG_ROOT`.
If you already have the container open, use **Dev Containers: Rebuild Container**
to pick up the setup or a changed baseline. The native preset uses vcpkg's static
Linux triplet for the host architecture; it does not cross-compile.

```sh
cmake --preset linux-native
cmake --build build/linux-native
ctest --test-dir build/linux-native --output-on-failure
```

The shared object is `build/linux-native/SlasconeClient.so` (no `lib` prefix).
Build in a separate build tree for each architecture.

After configuring, use **Ctrl+Shift+B** in VS Code to run the default
**Build linux-native** task. A toolchain error naming
`/scripts/buildsystems/vcpkg.cmake` means `VCPKG_ROOT` was unset or empty.
Rebuild an older dev container or, outside the container, set `VCPKG_ROOT` in
the shell that runs CMake. If the build tree contains an old toolchain path,
refresh the configuration with `cmake --fresh --preset linux-native`
(CMake 3.24 or later), or configure in a fresh build tree. If CMake still reports
that it cannot find Ninja, check `ninja --version` in the same shell.

## Linking from C

Include `SlasconeClient.h`. Link against the generated `SlasconeClient.lib`
import library on Windows and place the DLL where the loader can find it.
On Linux, link to `SlasconeClient.so` by path (not `-lSlasconeClient`, which
searches for a `lib` prefix) and make it available on the loader's search path.

Call `client_create(NULL)` to obtain an opaque `SlasconeClient *` without
setting a product ID. It returns
`NULL` if allocation fails. Release each successfully created handle once
with `client_destroy()`, not `free()`. `client_destroy(NULL)` does nothing.

XML and crypto libraries are linked statically. The library uses the static
MSVC runtime on Windows and statically links GCC's C++ and compiler runtimes
on Linux. The platform's system libraries (including glibc on Linux) remain
dynamic.
