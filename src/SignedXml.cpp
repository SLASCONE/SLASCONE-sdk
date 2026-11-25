#include "SignedXml.h"

#include <libxml/parser.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <xmlsec/crypto.h>
#include <xmlsec/errors.h>
#include <xmlsec/openssl/evp.h>
#include <xmlsec/xmldsig.h>

#include <filesystem>
#include <fstream>
#include <limits>

namespace slascone {
namespace {

class XmlSecurityRuntime {
public:
    XmlSecurityRuntime()
    {
        xmlInitParser();
        if (xmlSecInit() < 0)
            return;
        xmlSecErrorsDefaultCallbackEnableOutput(0);
        initialized_ = true;
        if (xmlSecCheckVersion() != 1 || xmlSecCryptoAppInit(nullptr) < 0)
            return;
        app_initialized_ = true;
        ready = xmlSecCryptoInit() >= 0;
    }

    ~XmlSecurityRuntime()
    {
        if (ready)
            xmlSecCryptoShutdown();
        if (app_initialized_)
            xmlSecCryptoAppShutdown();
        if (initialized_)
            xmlSecShutdown();
    }

    bool ready = false;

private:
    bool initialized_ = false;
    bool app_initialized_ = false;
};

bool is_dsig_element(xmlNodePtr node, const char *name)
{
    return node->type == XML_ELEMENT_NODE && node->ns &&
           xmlStrEqual(node->ns->href, xmlSecDSigNs) &&
           xmlStrEqual(node->name, BAD_CAST name);
}

xmlNodePtr only_dsig_child(xmlNodePtr parent, const char *name)
{
    xmlNodePtr match = nullptr;
    for (auto node = parent->children; node; node = node->next) {
        if (is_dsig_element(node, name)) {
            if (match)
                return nullptr;
            match = node;
        }
    }
    return match;
}

bool has_attribute(xmlNodePtr node, const char *name, const char *value)
{
    const auto free_xml = xmlFree;
    std::unique_ptr<xmlChar, decltype(free_xml)> attr(
        xmlGetNoNsProp(node, BAD_CAST name), free_xml);
    return attr && xmlStrEqual(attr.get(), BAD_CAST value);
}

bool has_algorithm(xmlNodePtr parent, const char *element, const char *algorithm)
{
    const auto node = only_dsig_child(parent, element);
    return node && has_attribute(node, "Algorithm", algorithm);
}

bool supported_signature(xmlNodePtr signature)
{
    // The only reference must cover the document minus this enveloped signature.
    const auto info = only_dsig_child(signature, "SignedInfo");
    if (!info || !has_algorithm(info, "CanonicalizationMethod",
                               "http://www.w3.org/TR/2001/REC-xml-c14n-20010315") ||
        !has_algorithm(info, "SignatureMethod", "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256"))
        return false;

    const auto reference = only_dsig_child(info, "Reference");
    if (!reference || !has_attribute(reference, "URI", "") ||
        !has_algorithm(reference, "DigestMethod", "http://www.w3.org/2001/04/xmlenc#sha256"))
        return false;

    const auto transforms = only_dsig_child(reference, "Transforms");
    if (!transforms || !has_algorithm(transforms, "Transform",
                                      "http://www.w3.org/2000/09/xmldsig#enveloped-signature"))
        return false;

    for (auto node = signature->children; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE && node != info && !is_dsig_element(node, "SignatureValue"))
            return false;
    }
    return true;
}

SlasconeStatus verify_signature(xmlNodePtr root, const VerificationKey &key)
{
    const auto signature = only_dsig_child(root, "Signature");
    if (!signature || !supported_signature(signature))
        return SLASCONE_INVALID_SIGNATURE;

    std::unique_ptr<xmlSecDSigCtx, decltype(&xmlSecDSigCtxDestroy)> ctx(
        xmlSecDSigCtxCreate(nullptr), xmlSecDSigCtxDestroy);
    if (!ctx)
        return SLASCONE_INTERNAL_ERROR;

    ctx->enabledReferenceUris = xmlSecTransformUriTypeEmpty;
    if (xmlSecDSigCtxEnableSignatureTransform(ctx.get(), xmlSecTransformInclC14NId) < 0 ||
        xmlSecDSigCtxEnableSignatureTransform(ctx.get(), xmlSecTransformRsaSha256Id) < 0 ||
        xmlSecDSigCtxEnableReferenceTransform(ctx.get(), xmlSecTransformEnvelopedId) < 0 ||
        xmlSecDSigCtxEnableReferenceTransform(ctx.get(), xmlSecTransformSha256Id) < 0)
        return SLASCONE_INTERNAL_ERROR;

    ctx->signKey = xmlSecKeyDuplicate(key.get());
    if (!ctx->signKey)
        return SLASCONE_INTERNAL_ERROR;
    if (xmlSecDSigCtxVerify(ctx.get(), signature) < 0 || ctx->status != xmlSecDSigStatusSucceeded)
        return SLASCONE_INVALID_SIGNATURE;
    return SLASCONE_OK;
}

}

SlasconeStatus load_verification_key(const std::string &pem, VerificationKey &key)
{
    static const XmlSecurityRuntime runtime;
    if (!runtime.ready)
        return SLASCONE_INTERNAL_ERROR;
    if (pem.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        return SLASCONE_INVALID_PUBLIC_KEY;

    std::unique_ptr<BIO, decltype(&BIO_free)> input(
        BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())), BIO_free);
    if (!input)
        return SLASCONE_INTERNAL_ERROR;
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> public_key(
        PEM_read_bio_PUBKEY(input.get(), nullptr, nullptr, nullptr), EVP_PKEY_free);
    if (!public_key || EVP_PKEY_base_id(public_key.get()) != EVP_PKEY_RSA) {
        ERR_clear_error();
        return SLASCONE_INVALID_PUBLIC_KEY;
    }

    key.reset(xmlSecKeyCreate());
    if (!key)
        return SLASCONE_INTERNAL_ERROR;
    std::unique_ptr<xmlSecKeyData, decltype(&xmlSecKeyDataDestroy)> data(
        xmlSecOpenSSLEvpKeyAdopt(public_key.get()), xmlSecKeyDataDestroy);
    if (!data)
        return SLASCONE_INTERNAL_ERROR;
    public_key.release();
    if (xmlSecKeySetValue(key.get(), data.get()) < 0)
        return SLASCONE_INTERNAL_ERROR;
    data.release();
    return SLASCONE_OK;
}

SlasconeStatus read_signed_xml(const char *path, const VerificationKey &key, XmlDocument &document)
{
    const auto file_path = std::filesystem::u8path(path);
    std::error_code error;
    if (!std::filesystem::is_regular_file(file_path, error))
        return SLASCONE_FILE_ERROR;
    std::ifstream input(file_path, std::ios::binary);
    if (!input)
        return SLASCONE_FILE_ERROR;

    constexpr std::size_t max_xml_bytes = 16 * 1024 * 1024;
    std::string xml;
    char buffer[4096];
    while (input.read(buffer, sizeof buffer) || input.gcount() > 0) {
        const auto count = static_cast<std::size_t>(input.gcount());
        if (count > max_xml_bytes - xml.size())
            return SLASCONE_INVALID_XML;
        xml.append(buffer, count);
    }
    if (input.bad() || !input.eof())
        return SLASCONE_FILE_ERROR;

    document.reset(xmlReadMemory(xml.data(), static_cast<int>(xml.size()), nullptr, nullptr,
                                 XML_PARSE_NONET | XML_PARSE_NO_XXE | XML_PARSE_NOERROR | XML_PARSE_NOWARNING));
    if (!document || document->intSubset || document->extSubset)
        return SLASCONE_INVALID_XML;
    const auto root = xmlDocGetRootElement(document.get());
    return root ? verify_signature(root, key) : SLASCONE_INVALID_XML;
}

}
