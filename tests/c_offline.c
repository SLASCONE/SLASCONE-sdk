#include "SlasconeClient.h"

#include <stdio.h>
#include <string.h>

static int expect(SlasconeStatus actual, SlasconeStatus expected, const char *expression)
{
    if (actual == expected)
        return 1;
    fprintf(stderr, "%s: expected status %d, got %d\n", expression, (int)expected, (int)actual);
    return 0;
}

#define EXPECT(expression, expected) \
    do { if (!expect((expression), (expected), #expression)) goto cleanup; } while (0)
#define READ(license, status) \
    EXPECT(read_offline_license(client, (license)), (status))
#define CHECK(license, activation, status) \
    do { READ((license), SLASCONE_OK); \
         EXPECT(check_offline_license(client, (activation)), (status)); } while (0)
#define FEATURE(handle, id, status, active) \
    do { if (!expect_feature((handle), (id), (status), (active))) goto cleanup; } while (0)

static int expect_feature(SlasconeClient *client, const char *id,
                          SlasconeStatus status, bool active)
{
    bool actual = !active;
    if (!expect(check_license_feature(client, id, &actual), status, id ? id : "NULL feature ID"))
        return 0;
    if (actual == active)
        return 1;
    fprintf(stderr, "Feature %s: expected active=%d, got %d\n", id ? id : "(null)", active, actual);
    return 0;
}

static int copy_inline_license(const char *path)
{
    FILE *source = fopen("inline.xml", "rb");
    if (!source)
        return 0;
    FILE *target = fopen(path, "wb");
    int ok = target != NULL;
    if (target) {
        char buffer[1024];
        size_t length;
        while (ok && (length = fread(buffer, 1, sizeof buffer, source)) != 0)
            ok = fwrite(buffer, 1, length, target) == length;
        ok = fclose(target) == 0 && ok && !ferror(source);
        if (!ok)
            remove(path);
    }
    fclose(source);
    return ok;
}

int main(int argc, char **argv)
{
    if (argc != 2)
        return 1;
    char pem[1024];
    FILE *file = fopen("public.pem", "rb");
    if (!file)
        return 1;
    const size_t length = fread(pem, 1, sizeof pem - 1, file);
    const int read_ok = feof(file) && !ferror(file);
    fclose(file);
    if (!read_ok || !length)
        return 1;
    pem[length] = '\0';

    SlasconeClient *client = client_create("product-abc");
    SlasconeClient *other = client_create("Other-Product");
    SlasconeClient *no_product = client_create(NULL);
    int result = 1;
    int copied = 0;
    if (!client || !other || !no_product)
        goto cleanup;

    EXPECT(set_client_id(NULL, "device-123"), SLASCONE_INVALID_ARGUMENT);
    EXPECT(set_pem_key(client, NULL), SLASCONE_INVALID_ARGUMENT);
    EXPECT(read_offline_license(NULL, "inline.xml"), SLASCONE_INVALID_ARGUMENT);
    EXPECT(check_offline_license(NULL, NULL), SLASCONE_INVALID_ARGUMENT);
    EXPECT(check_license_feature(client, "Reports", NULL), SLASCONE_INVALID_ARGUMENT);
    FEATURE(NULL, "Reports", SLASCONE_INVALID_ARGUMENT, false);
    FEATURE(client, NULL, SLASCONE_INVALID_ARGUMENT, false);
    FEATURE(client, "", SLASCONE_INVALID_ARGUMENT, false);
    READ(NULL, SLASCONE_INVALID_ARGUMENT);
    READ("inline.xml", SLASCONE_NOT_CONFIGURED);
    EXPECT(check_offline_license(client, NULL), SLASCONE_NOT_CONFIGURED);
    FEATURE(client, "Reports", SLASCONE_NOT_CONFIGURED, false);
    EXPECT(set_pem_key(client, "not a PEM"), SLASCONE_OK);
    READ("inline.xml", SLASCONE_INVALID_PUBLIC_KEY);
    EXPECT(set_pem_key(client, pem), SLASCONE_OK);
    if (!copy_inline_license(argv[1])) {
        fprintf(stderr, "Cannot copy inline license to %s\n", argv[1]);
        goto cleanup;
    }
    copied = 1;
    READ(argv[1], SLASCONE_OK);
    if (remove(argv[1]) != 0) {
        perror("Removing cached license source");
        goto cleanup;
    }
    copied = 0;
    FEATURE(client, "REPORTS", SLASCONE_OK, true);
    FEATURE(client, "Dashboard", SLASCONE_OK, false);
    FEATURE(client, "Export", SLASCONE_OK, true);
    FEATURE(client, "Import", SLASCONE_OK, false);
    FEATURE(client, "Unknown", SLASCONE_OK, false);
    EXPECT(check_offline_license(client, NULL), SLASCONE_NOT_CONFIGURED);
    EXPECT(set_pem_key(other, pem), SLASCONE_OK);
    EXPECT(set_client_id(other, "device-123"), SLASCONE_OK);
    FEATURE(other, "Reports", SLASCONE_NOT_CONFIGURED, false);
    EXPECT(check_offline_license(other, NULL), SLASCONE_NOT_CONFIGURED);
    EXPECT(read_offline_license(other, "inline.xml"), SLASCONE_OK);
    EXPECT(check_offline_license(other, NULL), SLASCONE_PRODUCT_ID_MISMATCH);
    FEATURE(other, "Reports", SLASCONE_OK, true);
    EXPECT(set_pem_key(no_product, pem), SLASCONE_OK);
    EXPECT(set_client_id(no_product, "device-123"), SLASCONE_OK);
    EXPECT(read_offline_license(no_product, "inline.xml"), SLASCONE_OK);
    EXPECT(check_offline_license(no_product, NULL), SLASCONE_NOT_CONFIGURED);

    char id[] = "device-123";
    EXPECT(set_client_id(client, id), SLASCONE_OK);
    id[0] = 'x';
    EXPECT(set_pem_key(client, pem), SLASCONE_OK);
    EXPECT(check_offline_license(client, NULL), SLASCONE_OK);
    EXPECT(check_offline_license(client, ""), SLASCONE_OK);
    EXPECT(check_offline_license(client, "nonexistent.xml"), SLASCONE_FILE_ERROR);
    FEATURE(client, "Reports", SLASCONE_OK, true);
    READ("nonexistent.xml", SLASCONE_FILE_ERROR);
    EXPECT(check_offline_license(client, NULL), SLASCONE_NOT_CONFIGURED);
    FEATURE(client, "Reports", SLASCONE_NOT_CONFIGURED, false);
    FEATURE(other, "Reports", SLASCONE_OK, true);

    CHECK("license.xml", "activation.xml", SLASCONE_RELEASE_NOT_ALLOWED);
    FEATURE(client, "Reports", SLASCONE_OK, false);
    EXPECT(set_software_release(client, "v26.3.1"), SLASCONE_OK);
    CHECK("license.xml", "activation.xml", SLASCONE_OK);
    CHECK("license.xml", NULL, SLASCONE_CLIENT_ID_MISMATCH);
    CHECK("inline.xml", "activation-no-revision.xml", SLASCONE_OK);
    CHECK("license.xml", "activation-no-revision.xml", SLASCONE_REVISION_MISMATCH);
    CHECK("license.xml", "activation-wrong-revision.xml", SLASCONE_REVISION_MISMATCH);
    CHECK("license.xml", "activation-wrong-key.xml", SLASCONE_LICENSE_KEY_MISMATCH);
    READ("tampered-license.xml", SLASCONE_INVALID_SIGNATURE);
    FEATURE(client, "Reports", SLASCONE_NOT_CONFIGURED, false);
    CHECK("license.xml", "tampered-activation.xml", SLASCONE_INVALID_SIGNATURE);
    READ("duplicate-field.xml", SLASCONE_INVALID_XML);
    READ("with-dtd.xml", SLASCONE_INVALID_XML);
    READ("missing-product.xml", SLASCONE_INVALID_XML);
    READ("invalid-feature.xml", SLASCONE_INVALID_XML);
    READ("duplicate-feature.xml", SLASCONE_INVALID_XML);

    EXPECT(set_software_release(client, "26.2"), SLASCONE_OK);
    CHECK("license.xml", "activation.xml", SLASCONE_OK);
    EXPECT(set_software_release(client, "26.10"), SLASCONE_OK);
    CHECK("license.xml", "activation.xml", SLASCONE_RELEASE_NOT_ALLOWED);
    EXPECT(set_software_release(client, "26"), SLASCONE_OK);
    CHECK("license.xml", "activation.xml", SLASCONE_RELEASE_NOT_ALLOWED);
    EXPECT(set_software_release(client, "26.3!"), SLASCONE_OK);
    CHECK("license.xml", "activation.xml", SLASCONE_RELEASE_NOT_ALLOWED);

    CHECK("invalid-date.xml", NULL, SLASCONE_INVALID_LICENSE_DATES);
    CHECK("expired.xml", NULL, SLASCONE_LICENSE_EXPIRED);
    CHECK("future.xml", NULL, SLASCONE_LICENSE_NOT_YET_VALID);
    EXPECT(set_client_id(client, "other"), SLASCONE_OK);
    CHECK("inline.xml", NULL, SLASCONE_CLIENT_ID_MISMATCH);
    FEATURE(client, "Reports", SLASCONE_OK, true);
    EXPECT(set_pem_key(client, "not a PEM"), SLASCONE_OK);
    FEATURE(client, "Reports", SLASCONE_NOT_CONFIGURED, false);
    EXPECT(check_offline_license(client, NULL), SLASCONE_NOT_CONFIGURED);
    EXPECT(set_pem_key(client, pem), SLASCONE_OK);
    memset(pem, 0, sizeof pem);
    READ("inline.xml", SLASCONE_OK);
    READ("", SLASCONE_INVALID_ARGUMENT);
    FEATURE(client, "Reports", SLASCONE_NOT_CONFIGURED, false);
    READ("inline.xml", SLASCONE_OK);
    EXPECT(set_pem_key(client, ""), SLASCONE_OK);
    FEATURE(client, "Reports", SLASCONE_NOT_CONFIGURED, false);
    READ("inline.xml", SLASCONE_NOT_CONFIGURED);
    result = 0;

cleanup:
    if (copied)
        remove(argv[1]);
    client_destroy(no_product);
    client_destroy(other);
    client_destroy(client);
    return result;
}
