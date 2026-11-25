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
#define CHECK(license, activation, status) \
    EXPECT(check_offline_license(client, (license), (activation)), (status))

int main(void)
{
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

    SlasconeClient *client = client_create(NULL);
    SlasconeClient *other = client_create(NULL);
    int result = 1;
    if (!client || !other)
        goto cleanup;

    EXPECT(set_client_id(NULL, "device-123"), SLASCONE_INVALID_ARGUMENT);
    EXPECT(set_pem_key(client, NULL), SLASCONE_INVALID_ARGUMENT);
    CHECK(NULL, NULL, SLASCONE_INVALID_ARGUMENT);
    CHECK("inline.xml", NULL, SLASCONE_NOT_CONFIGURED);
    char id[] = "device-123";
    EXPECT(set_client_id(client, id), SLASCONE_OK);
    id[0] = 'x';
    EXPECT(set_pem_key(client, "not a PEM"), SLASCONE_OK);
    CHECK("inline.xml", NULL, SLASCONE_INVALID_PUBLIC_KEY);
    EXPECT(set_pem_key(client, pem), SLASCONE_OK);
    memset(pem, 0, sizeof pem);
    CHECK("inline.xml", NULL, SLASCONE_OK);
    CHECK("inline.xml", "", SLASCONE_OK);
    EXPECT(check_offline_license(other, "inline.xml", NULL), SLASCONE_NOT_CONFIGURED);
    CHECK("nonexistent.xml", NULL, SLASCONE_FILE_ERROR);
    CHECK("inline.xml", "nonexistent.xml", SLASCONE_FILE_ERROR);

    CHECK("license.xml", "activation.xml", SLASCONE_RELEASE_NOT_ALLOWED);
    EXPECT(set_software_release(client, "v26.3.1"), SLASCONE_OK);
    CHECK("license.xml", "activation.xml", SLASCONE_OK);
    CHECK("license.xml", NULL, SLASCONE_CLIENT_ID_MISMATCH);
    CHECK("inline.xml", "activation-no-revision.xml", SLASCONE_OK);
    CHECK("license.xml", "activation-no-revision.xml", SLASCONE_REVISION_MISMATCH);
    CHECK("license.xml", "activation-wrong-revision.xml", SLASCONE_REVISION_MISMATCH);
    CHECK("license.xml", "activation-wrong-key.xml", SLASCONE_LICENSE_KEY_MISMATCH);
    CHECK("tampered-license.xml", "activation.xml", SLASCONE_INVALID_SIGNATURE);
    CHECK("license.xml", "tampered-activation.xml", SLASCONE_INVALID_SIGNATURE);
    CHECK("duplicate-field.xml", NULL, SLASCONE_INVALID_XML);
    CHECK("with-dtd.xml", NULL, SLASCONE_INVALID_XML);

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
    EXPECT(set_pem_key(client, ""), SLASCONE_OK);
    CHECK("inline.xml", NULL, SLASCONE_NOT_CONFIGURED);
    result = 0;

cleanup:
    client_destroy(other);
    client_destroy(client);
    return result;
}
