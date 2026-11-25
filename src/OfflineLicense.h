#ifndef SLASCONE_OFFLINE_LICENSE_H
#define SLASCONE_OFFLINE_LICENSE_H

#include "SlasconeClient.h"

#include <string>

namespace slascone {

SlasconeStatus check_license(const char *license_path, const char *activation_path,
                             const std::string &client_id, const std::string &release,
                             const std::string &pem);

}

#endif
