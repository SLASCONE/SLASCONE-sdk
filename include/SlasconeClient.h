#ifndef SLASCONE_CLIENT_H
#define SLASCONE_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(SLASCONE_CLIENT_BUILDING)
#    define SLASCONE_CLIENT_API __declspec(dllexport)
#  else
#    define SLASCONE_CLIENT_API __declspec(dllimport)
#  endif
#else
#  define SLASCONE_CLIENT_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

SLASCONE_CLIENT_API uint32_t slascone_client_abi_version(void);

typedef struct SlasconeClient SlasconeClient;

/* Returns an owned client, or NULL if allocation fails. */
SLASCONE_CLIENT_API SlasconeClient *client_create(const char *product_id);

/* Releases a client returned by client_create(). Passing NULL is a no-op. */
SLASCONE_CLIENT_API void client_destroy(SlasconeClient *client);

typedef enum SlasconeStatus {
    SLASCONE_OK = 0,
    SLASCONE_INVALID_ARGUMENT = 1,
    SLASCONE_OUT_OF_MEMORY = 2,
    SLASCONE_INTERNAL_ERROR = 3,
    SLASCONE_NOT_CONFIGURED = 4,
    SLASCONE_INVALID_PUBLIC_KEY = 5,
    SLASCONE_FILE_ERROR = 6,
    SLASCONE_INVALID_XML = 7,
    SLASCONE_INVALID_SIGNATURE = 8,
    SLASCONE_LICENSE_KEY_MISMATCH = 9,
    SLASCONE_REVISION_MISMATCH = 10,
    SLASCONE_CLIENT_ID_MISMATCH = 11,
    SLASCONE_RELEASE_NOT_ALLOWED = 12,
    SLASCONE_INVALID_LICENSE_DATES = 13,
    SLASCONE_LICENSE_NOT_YET_VALID = 14,
    SLASCONE_LICENSE_EXPIRED = 15,
    SLASCONE_PRODUCT_ID_MISMATCH = 16
} SlasconeStatus;

/* Values are copied; NULL is invalid and an empty string clears the setting. */
SLASCONE_CLIENT_API SlasconeStatus set_client_id(SlasconeClient *client, const char *value);
SLASCONE_CLIENT_API SlasconeStatus set_software_release(SlasconeClient *client, const char *value);
SLASCONE_CLIENT_API SlasconeStatus set_pem_key(SlasconeClient *client, const char *value);

/* Verifies and caches the signed license at a UTF-8 path. Failure clears the
 * cache, as does changing the PEM key. */
SLASCONE_CLIENT_API SlasconeStatus read_offline_license(
    SlasconeClient *client, const char *license_path);

/* Checks the cached license. A NULL or empty UTF-8 path selects inline activation. */
SLASCONE_CLIENT_API SlasconeStatus check_offline_license(
    SlasconeClient *client, const char *activation_path);

/* Reads a signed flag, not an activation result. Missing features are inactive. */
SLASCONE_CLIENT_API SlasconeStatus check_license_feature(
    SlasconeClient *client, const char *feature_id, bool *is_active);

#ifdef __cplusplus
}
#endif

#endif
