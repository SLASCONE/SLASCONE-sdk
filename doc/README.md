# SlasconeClient developer reference

This page describes the public C API and how to consume the shared library.
The public declarations are in [`include/SlasconeClient.h`](../include/SlasconeClient.h).

## Current scope

The library provides client handles and verification of offline SLASCONE
license files, plus lookup of signed feature flags. It does not contact the
service, check online revocation, or evaluate feature expiration.

## Public API

Include `SlasconeClient.h`. The header is valid C and can also be included from
C++. All exported functions use C linkage.

| Declaration | Behavior |
| --- | --- |
| `uint32_t slascone_client_abi_version(void)` | Returns the library ABI version, currently `2`. |
| `SlasconeClient *client_create(const char *product_id)` | Creates an opaque client handle. Pass the expected product ID for license checks. `NULL` or `""` leaves it unset. Returns `NULL` if the handle cannot be created. |
| `void client_destroy(SlasconeClient *client)` | Releases a handle created by `client_create`. Passing `NULL` is a no-op. |
| `SlasconeStatus set_client_id(SlasconeClient *client, const char *value)` | Stores the expected client ID. |
| `SlasconeStatus set_software_release(SlasconeClient *client, const char *value)` | Stores the running software release. Needed only when the license restricts the release. |
| `SlasconeStatus set_pem_key(SlasconeClient *client, const char *value)` | Stores the trusted RSA public key in PEM `PUBLIC KEY` format. Changing or clearing the value discards the cached license. |
| `SlasconeStatus read_offline_license(SlasconeClient *client, const char *license_path)` | Verifies the license signature and caches the parsed license. The path is UTF-8. Requires a PEM key but not a client ID. A failed read clears the cached license. |
| `SlasconeStatus check_offline_license(SlasconeClient *client, const char *activation_path)` | Checks the cached license against the configured product and client IDs and the activation data. A UTF-8 path selects a separately verified activation file; `NULL` or `""` selects inline activation. Never reopens the license file. |
| `SlasconeStatus check_license_feature(SlasconeClient *client, const char *feature_id, bool *is_active)` | Looks up a nonempty feature ID in the cached license using ASCII case-insensitive comparison. Missing or inactive features return `SLASCONE_OK` with `false`. A non-null output is set to `false` on error. |

`SlasconeClient` is opaque: callers must not inspect or allocate it directly.
Destroy each successfully created handle exactly once with `client_destroy()`;
do not use `free()` or `delete`.

Setters copy their NUL-terminated strings. An empty string clears a setting;
`NULL` is invalid. Set the PEM key from a trusted source, never from the license
being verified. A license check requires a cached license and nonempty product
and client IDs. A feature query requires only a cached license. Missing
prerequisites return `SLASCONE_NOT_CONFIGURED`. Callers must synchronize
mutation or destruction of a shared handle.

`SLASCONE_OK` (zero) is the only success status. Other statuses distinguish
invalid arguments or missing configuration, a bad key, unreadable files,
invalid XML or signatures, identifier mismatches, and release or date
failures. See the enum in the public header for the full list.

### What is checked

- Each supplied XML file must have a valid enveloped RSA-SHA256 signature
  using canonical XML 1.0 and a SHA-256 digest over the whole document.
  External references and DTDs are not supported. Files are limited to 16 MiB.
- The license must contain a nonempty `product_id`, and it must match the
  product ID passed to `client_create()`. A mismatch returns
  `SLASCONE_PRODUCT_ID_MISMATCH`. License and separate activation keys
  must match. The revision IDs must match, or both must be missing or empty.
  The configured client ID must
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
- Reading the license rejects malformed feature entries and duplicate IDs.
  `<is_active>` accepts XML Schema booleans (`true`/`1` and `false`/`0`).
  A feature lookup reports the signed flag even before activation or after a
  failed license check. To grant access, require both a successful license
  check and an active feature.

## Offline verification from C

```c
#include "SlasconeClient.h"
#include <stddef.h>

SlasconeStatus verify_inline_feature(const char *trusted_public_key, const char *product_id,
                                     const char *device_id, const char *license_path,
                                     const char *feature_id, bool *is_active)
{
    if (is_active == NULL)
        return SLASCONE_INVALID_ARGUMENT;
    *is_active = false;
    SlasconeClient *client = client_create(product_id);
    if (client == NULL)
        return SLASCONE_OUT_OF_MEMORY;

    SlasconeStatus status = set_client_id(client, device_id);
    if (status == SLASCONE_OK)
        status = set_software_release(client, "26.3");
    if (status == SLASCONE_OK)
        status = set_pem_key(client, trusted_public_key);
    if (status == SLASCONE_OK)
        status = read_offline_license(client, license_path);
    if (status == SLASCONE_OK)
        status = check_offline_license(client, NULL);
    if (status == SLASCONE_OK)
        status = check_license_feature(client, feature_id, is_active);
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

The reported ABI version is `2`. ABI 1 callers must rebuild against the new
header and deploy with a matching library: `check_offline_license()` still has
the same export name but now takes only the handle and activation path.
Pass the license path to `read_offline_license()` first. Check the version
before calling ABI-dependent functions when loading the library dynamically.
