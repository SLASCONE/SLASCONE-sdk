# SlasconeClient developer reference

This page describes the public C API and how to consume the shared library.
The public declarations are in [`include/SlasconeClient.h`](../include/SlasconeClient.h).

## Current scope

The library provides client handles and verification of offline SLASCONE
license files. It does not contact the service or check online revocation
or feature-level validity.

## Public API

Include `SlasconeClient.h`. The header is valid C and can also be included from
C++. All exported functions use C linkage.

| Declaration | Behavior |
| --- | --- |
| `uint32_t slascone_client_abi_version(void)` | Returns the library ABI version, currently `1`. |
| `SlasconeClient *client_create(const char *product_id)` | Creates an opaque client handle. Pass `NULL` to leave the product ID unset. Returns `NULL` if handle allocation fails. |
| `void client_destroy(SlasconeClient *client)` | Releases a handle created by `client_create`. Passing `NULL` is a no-op. |
| `SlasconeStatus set_client_id(SlasconeClient *client, const char *value)` | Stores the expected client ID. |
| `SlasconeStatus set_software_release(SlasconeClient *client, const char *value)` | Stores the running software release. Needed only when the license restricts the release. |
| `SlasconeStatus set_pem_key(SlasconeClient *client, const char *value)` | Stores the trusted RSA public key in PEM `PUBLIC KEY` format. The key is parsed during verification. |
| `SlasconeStatus check_offline_license(SlasconeClient *client, const char *license_path, const char *activation_path)` | Verifies license XML and, if supplied, activation XML. Paths are UTF-8; use `NULL` or `""` for an inline activation. |

`SlasconeClient` is opaque: callers must not inspect or allocate it directly.
Destroy each successfully created handle exactly once with `client_destroy()`;
do not use `free()` or `delete`.

Setters copy their NUL-terminated strings. An empty string clears a setting;
`NULL` is invalid. Configure a client ID and PEM key before checking a license.
Set the key from a trusted source, never from the license being verified.
Callers must synchronize mutation or destruction of a shared handle.

`SLASCONE_OK` (zero) is the only success status. Other statuses distinguish
invalid arguments or missing configuration, a bad key, unreadable files,
invalid XML or signatures, identifier mismatches, and release or date
failures. See the enum in the public header for the full list.

### What is checked

- Each supplied XML file must have a valid enveloped RSA-SHA256 signature
  using canonical XML 1.0 and a SHA-256 digest over the whole document.
  External references and DTDs are not supported. Files are limited to 16 MiB.
- License and separate activation keys must match. The revision IDs must
  match, or both must be missing or empty. The configured client ID must
  match the client ID in the activation file (or in the license for inline
  activation). These identifiers are compared case-insensitively for ASCII.
- A nonempty release limit is an inclusive numeric ceiling. For example,
  limit `26.3` allows `26.3.1` but rejects `26.10`. The running release
  must have at least as many components as the limit. As in the demo,
  ASCII letters are removed from the configured release before comparison.
- Both license dates must be present and valid in UTC
  (`YYYY-MM-DDTHH:MM:SS[.fraction]Z`). The current system time must be
  between them, inclusive. Year 9999 and up to nine fractional digits
  are supported.

## Offline verification from C

```c
#include "SlasconeClient.h"
#include <stddef.h>

SlasconeStatus verify_inline(const char *trusted_public_key, const char *device_id,
                             const char *license_path)
{
    SlasconeClient *client = client_create(NULL);
    if (client == NULL)
        return SLASCONE_OUT_OF_MEMORY;

    SlasconeStatus status = set_client_id(client, device_id);
    if (status == SLASCONE_OK)
        status = set_software_release(client, "26.3");
    if (status == SLASCONE_OK)
        status = set_pem_key(client, trusted_public_key);
    if (status == SLASCONE_OK)
        status = check_offline_license(client, license_path, NULL);
    client_destroy(client);
    return status;
}
```

## Linking

Add the repository's `include/` directory to the C compiler's header search
path. Link the consumer against the shared library:

- **Windows:** link the generated `SlasconeClient.lib` import library and make
  `SlasconeClient.dll` available to the Windows loader at runtime.
- **Linux:** link to the generated `SlasconeClient.so` by its path (it has no
  `lib` prefix) and make it available on the runtime loader's search path.

For example, with GCC and a consumer source file named `consumer.c`, a local
build can be linked with:

```sh
gcc -Iinclude consumer.c build/linux-native/SlasconeClient.so \
    -Wl,-rpath,'$ORIGIN/build/linux-native' -o consumer
```

See the top-level [README](../README.md) for configure, build, and CTest
commands for the supported Windows and Linux presets. The library links its
XML and crypto dependencies and compiler runtimes statically, while platform
system libraries remain dynamic.

## Export and ABI notes

`SLASCONE_CLIENT_API` marks declarations for export when building the library
and import when consuming it on Windows. Consumers should include the header
without defining `SLASCONE_CLIENT_BUILDING`; that definition is reserved for
the library build.

The reported ABI version is currently `1`. Check it at runtime if the
application needs to reject libraries with an incompatible ABI. The version
does not indicate whether service functionality is available.
