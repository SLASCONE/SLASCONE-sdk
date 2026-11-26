# SlasconeClient

A C++17 shared library with a C ABI. `slascone_client_abi_version()` returns
`2`. The library reads and caches signed SLASCONE offline licenses, checks
inline or separate activation, and exposes signed feature flags. Offline
verification does not contact the service.

For AI-assisted development, see the [agent guide](./AGENTS.md) for repository
conventions, compatibility rules, and verification expectations.

For the public API and integration details, see the [developer reference](./doc/README.md).

## Table of Contents

* [SLASCONE Licensing Service](#slascone-licensing-service)
* [What This Library Does](#what-this-library-does)
* [Build Dependencies](#build-dependencies)
* [Windows (x86-64 and ARM64)](#windows-x86-64-and-arm64)
* [Linux (x86-64 and ARM64)](#linux-x86-64-and-arm64)
* [Linking from C](#linking-from-c)
* [Further Reading](#further-reading)

## SLASCONE Licensing Service

SLASCONE provides centralized license and entitlement management for software
products. Applications can use the SLASCONE API or a platform-specific client
to activate licenses, send license heartbeats, retrieve current entitlements,
and report usage or analytics. Depending on the product's configuration,
licensing workflows can include device-based or named-user licensing, floating
sessions, and online or offline use.

The demo repositories show these workflows in different application types.
They connect to a SLASCONE environment to perform online operations and
demonstrate patterns such as signed license files and locally cached license
data for offline use. The exact workflows available depend on how the SLASCONE
environment and product are configured.

## What This Library Does

This repository provides a small C ABI library for verifying signed SLASCONE
offline license and activation XML files and reading their signed feature
flags. It does not call the SLASCONE service, activate licenses, send
heartbeats, or check online revocation. An application that needs online
licensing or analytics must use an appropriate SLASCONE API or client
integration separately.

For offline verification, provide the trusted public key and expected product
and client identifiers, read the license, then check it against its inline or
separate activation data. Only grant a feature when the license check succeeds
and the feature is active. See the [developer reference](./doc/README.md) for
the API details and a C example.

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

Call `client_create(product_id)` with the expected product ID for offline
verification. Release each successfully created handle once with
`client_destroy()`, not `free()`. `client_destroy(NULL)` does nothing.
ABI 1 consumers must rebuild: ABI 2 moves the license path from
`check_offline_license()` to `read_offline_license()`.

XML and crypto libraries are linked statically. The library uses the static
MSVC runtime on Windows and statically links GCC's C++ and compiler runtimes
on Linux. The platform's system libraries (including glibc on Linux) remain
dynamic.

## Further Reading

* [SLASCONE website](https://slascone.com/)
* [SLASCONE Help Center](https://support.slascone.com/)
* [API Test Center](https://api365.slascone.com/swagger)
* [SLASCONE C# demo](https://github.com/SLASCONE/SLASCONE-demo-csharp-nuget)
* [SLASCONE WPF demo](https://github.com/SLASCONE/SLASCONE-demo-wpf-nuget)
* [SLASCONE Java demo](https://github.com/SLASCONE/SLASCONE-demo-java)
* [SLASCONE C++ demo](https://github.com/SLASCONE/SLASCONE-demo-cpp)
