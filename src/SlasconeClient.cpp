#include "SlasconeClient.h"
#include "OfflineLicense.h"

#include <filesystem>
#include <new>
#include <stdexcept>
#include <string>

struct SlasconeClient {
    std::string product_id;
    std::string client_id;
    std::string software_release;
    std::string pem_key;
};

namespace {

SlasconeStatus set_value(SlasconeClient *client, const char *value,
                         std::string SlasconeClient::*member)
{
    if (!client || !value)
        return SLASCONE_INVALID_ARGUMENT;
    try {
        (client->*member).assign(value);
        return SLASCONE_OK;
    } catch (const std::bad_alloc &) {
        return SLASCONE_OUT_OF_MEMORY;
    } catch (const std::length_error &) {
        return SLASCONE_INVALID_ARGUMENT;
    }
}

}

uint32_t slascone_client_abi_version(void)
{
    return 1;
}

SlasconeClient *client_create(const char *product_id)
{
    SlasconeClient *client = new (std::nothrow) SlasconeClient{};
    if (client && product_id)
        client->product_id.assign(product_id);
    return client;
}

void client_destroy(SlasconeClient *client)
{
    delete client;
}

SlasconeStatus set_client_id(SlasconeClient *client, const char *value)
{
    return set_value(client, value, &SlasconeClient::client_id);
}

SlasconeStatus set_software_release(SlasconeClient *client, const char *value)
{
    return set_value(client, value, &SlasconeClient::software_release);
}

SlasconeStatus set_pem_key(SlasconeClient *client, const char *value)
{
    return set_value(client, value, &SlasconeClient::pem_key);
}

SlasconeStatus check_offline_license(
    SlasconeClient *client, const char *license_path, const char *activation_path)
{
    if (!client || !license_path || !*license_path)
        return SLASCONE_INVALID_ARGUMENT;
    if (client->client_id.empty() || client->pem_key.empty())
        return SLASCONE_NOT_CONFIGURED;

    try {
        return slascone::check_license(license_path, activation_path, client->client_id,
                                       client->software_release, client->pem_key);
    } catch (const std::bad_alloc &) {
        return SLASCONE_OUT_OF_MEMORY;
    } catch (const std::filesystem::filesystem_error &) {
        return SLASCONE_FILE_ERROR;
    } catch (const std::exception &) {
        // Translate implementation failures at the C ABI boundary.
        return SLASCONE_INTERNAL_ERROR;
    }
}
