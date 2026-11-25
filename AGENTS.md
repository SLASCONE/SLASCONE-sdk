# SlasconeClient agent guide

## Where to work

- [`include/SlasconeClient.h`](./include/SlasconeClient.h) is the public C ABI.
- [`src/`](./src/) contains the C++17 implementation, including offline verification. It does not call the SLASCONE service.
- [`tests/`](./tests/) contains C consumers that link to the shared library and run through CTest.

## Compatibility rules

- Keep public headers valid C. Use `SLASCONE_CLIENT_API` for public exports and keep their `extern "C"` linkage. Do not leak C++ types or exceptions across the ABI. Preserve existing signatures and behavior unless an ABI change is explicitly requested.
- Keep support for Windows/MSVC and Linux/GCC on x86-64 and ARM64. Keep the exact output names `SlasconeClient.dll` and `SlasconeClient.so` (no `lib` prefix).
- Preserve static compiler runtimes. Do not add a third-party dependency without approval; if added, link it statically. Platform system libraries may remain dynamic.
- The approved XML signature dependencies are pinned in [`vcpkg.json`](./vcpkg.json). Use the static triplets documented in the README.

## Verify changes

Use the platform commands in the [README](./README.md): configure with `windows-x64`, `windows-arm64`, or `linux-native`, build in the preset's directory under `build/`, then run CTest. The Linux dev container uses the native preset; it does not cross-compile. Run an ARM64 binary on a compatible host.

For API changes, verify the affected behavior from a C caller, not only from C++. Report which platforms and architectures were actually tested and which could not be tested.

## Documentation

Keep this `AGENTS.md` aligned with repository workflow and development conventions, and keep the [developer reference](./doc/README.md) aligned with the public API and usage. Update either document alongside changes that would otherwise make it inaccurate or incomplete.

## Keep the task focused

For public API, dependency, or build-matrix changes, agree on scope, acceptance criteria, and verification before implementation. Read only the files relevant to the task; skip generated files and build output except for the specific failures being investigated. Leave unrelated changes alone and use only the tools needed for the task.
