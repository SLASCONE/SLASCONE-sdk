#ifndef SLASCONE_SIGNED_XML_H
#define SLASCONE_SIGNED_XML_H

#include "SlasconeClient.h"

#include <libxml/tree.h>
#include <xmlsec/xmlsec.h>
#include <xmlsec/keys.h>

#include <memory>
#include <string>

namespace slascone {

using XmlDocument = std::unique_ptr<xmlDoc, decltype(&xmlFreeDoc)>;
using VerificationKey = std::unique_ptr<xmlSecKey, decltype(&xmlSecKeyDestroy)>;

SlasconeStatus load_verification_key(const std::string &pem, VerificationKey &key);
SlasconeStatus read_signed_xml(const char *path, const VerificationKey &key, XmlDocument &document);

}

#endif
