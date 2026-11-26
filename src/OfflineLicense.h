#ifndef SLASCONE_OFFLINE_LICENSE_H
#define SLASCONE_OFFLINE_LICENSE_H

#include "SignedXml.h"

#include <map>
#include <string>
#include <string_view>

namespace slascone {

struct OfflineLicense {
    XmlDocument document{nullptr, xmlFreeDoc};
    std::string license_key;
    std::string revision;
    std::string product_id;
    std::map<std::string, bool> features;
};

SlasconeStatus read_license(const char *license_path, const std::string &pem, OfflineLicense &license);
SlasconeStatus check_license(const OfflineLicense &license, const char *activation_path,
                             const std::string &product_id, const std::string &client_id,
                             const std::string &release, const std::string &pem);
bool is_feature_active(const OfflineLicense &license, std::string_view feature_id);

}

#endif
