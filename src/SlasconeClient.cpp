#include "SlasconeClient.h"
#include "OfflineLicense.h"

#include <filesystem>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

struct SlasconeClient {
    std::string product_id;
    std::string client_id;
    std::string software_release;
    std::string pem_key;
    std::unique_ptr<slascone::OfflineLicense> license;
};

namespace {

template <typename Operation>
SlasconeStatus offline_status(Operation operation)
{
    try {
        return operation();
    } catch (const std::bad_alloc &) {
        return SLASCONE_OUT_OF_MEMORY;
    } catch (const std::filesystem::filesystem_error &) {
        return SLASCONE_FILE_ERROR;
    } catch (const std::exception &) {
        return SLASCONE_INTERNAL_ERROR;
    }
}

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
    return 2;
}

SlasconeClient *client_create(const char *product_id)
{
    try {
        auto client = std::make_unique<SlasconeClient>();
        if (product_id)
            client->product_id.assign(product_id);
        return client.release();
    } catch (const std::bad_alloc &) {
        return nullptr;
    } catch (const std::length_error &) {
        return nullptr;
    }
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
    if (!client || !value)
        return SLASCONE_INVALID_ARGUMENT;
    if (client->pem_key == value)
        return SLASCONE_OK;
    const auto status = set_value(client, value, &SlasconeClient::pem_key);
    if (status == SLASCONE_OK)
        client->license.reset();
    return status;
}

SlasconeStatus read_offline_license(SlasconeClient *client, const char *license_path)
{
    if (!client)
        return SLASCONE_INVALID_ARGUMENT;
    client->license.reset();
    if (!license_path || !*license_path)
        return SLASCONE_INVALID_ARGUMENT;
    if (client->pem_key.empty())
        return SLASCONE_NOT_CONFIGURED;

    return offline_status([&] {
        auto license = std::make_unique<slascone::OfflineLicense>();
        const auto status = slascone::read_license(license_path, client->pem_key, *license);
        if (status == SLASCONE_OK)
            client->license = std::move(license);
        return status;
    });
}

SlasconeStatus check_offline_license(SlasconeClient *client, const char *activation_path)
{
    if (!client)
        return SLASCONE_INVALID_ARGUMENT;
    if (!client->license || client->product_id.empty() || client->client_id.empty())
        return SLASCONE_NOT_CONFIGURED;

    return offline_status([&] {
        return slascone::check_license(*client->license, activation_path,
                                       client->product_id, client->client_id,
                                       client->software_release, client->pem_key);
    });
}

SlasconeStatus check_license_feature(SlasconeClient *client, const char *feature_id, bool *is_active)
{
    if (!is_active)
        return SLASCONE_INVALID_ARGUMENT;
    *is_active = false;
    if (!client || !feature_id || !*feature_id)
        return SLASCONE_INVALID_ARGUMENT;
    if (!client->license)
        return SLASCONE_NOT_CONFIGURED;

    return offline_status([&] {
        *is_active = slascone::is_feature_active(*client->license, feature_id);
        return SLASCONE_OK;
    });
}
