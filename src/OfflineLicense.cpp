#include "OfflineLicense.h"
#include "SignedXml.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace slascone {
namespace {

bool matches(xmlNodePtr node, const char *name)
{
    return node && node->type == XML_ELEMENT_NODE && !node->ns &&
           xmlStrEqual(node->name, BAD_CAST name);
}

bool find_field(xmlNodePtr parent, const char *name, xmlNodePtr &result)
{
    result = nullptr;
    for (auto node = parent->children; node; node = node->next) {
        if (!matches(node, name))
            continue;
        if (result)
            return false;
        result = node;
    }
    return true;
}

bool read_field(xmlNodePtr parent, const char *name, std::string &value)
{
    xmlNodePtr field;
    if (!find_field(parent, name, field))
        return false;
    value.clear();
    for (auto child = field ? field->children : nullptr; child; child = child->next) {
        if (child->type == XML_TEXT_NODE || child->type == XML_CDATA_SECTION_NODE) {
            if (child->content)
                value += reinterpret_cast<const char *>(child->content);
        } else if (child->type != XML_COMMENT_NODE && child->type != XML_PI_NODE) {
            return false;
        }
    }
    return true;
}

bool same_id(std::string_view left, std::string_view right)
{
    const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
    return std::equal(left.begin(), left.end(), right.begin(), right.end(),
                      [&](char l, char r) { return lower(l) == lower(r); });
}

bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

bool parse_release(std::string_view release, bool allow_letters, std::vector<int> &parts)
{
    std::string cleaned;
    if (allow_letters) {
        for (const auto c : release) {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
                cleaned += c;
        }
        release = cleaned;
    }

    for (;;) {
        const auto separator = release.find('.');
        const auto component = release.substr(0, separator);
        if (component.empty() || !std::all_of(component.begin(), component.end(), is_digit))
            return false;
        int value;
        const auto result = std::from_chars(component.data(), component.data() + component.size(), value);
        if (result.ec != std::errc{})
            return false;
        parts.push_back(value);
        if (separator == std::string_view::npos)
            return true;
        release.remove_prefix(separator + 1);
    }
}

SlasconeStatus check_release(xmlNodePtr license, const std::string &release)
{
    xmlNodePtr limitation;
    std::string maximum;
    if (!find_field(license, "software_release_limitation", limitation) ||
        (limitation && !read_field(limitation, "software_release", maximum)))
        return SLASCONE_INVALID_XML;
    if (maximum.empty())
        return SLASCONE_OK;

    std::vector<int> limit, current;
    if (!parse_release(maximum, false, limit))
        return SLASCONE_INVALID_XML;
    if (!parse_release(release, true, current) || current.size() < limit.size() ||
        std::lexicographical_compare(limit.begin(), limit.end(),
                                     current.begin(), current.begin() + limit.size()))
        return SLASCONE_RELEASE_NOT_ALLOWED;
    return SLASCONE_OK;
}

using UtcTime = std::pair<std::int64_t, int>;  // Unix seconds and nanoseconds.

constexpr bool leap_year(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

constexpr int days_before_year(int year)
{
    const int y = year - 1;
    return 365 * y + y / 4 - y / 100 + y / 400;
}

bool parse_utc(std::string_view text, UtcTime &time)
{
    if (text.size() < 20 || text.back() != 'Z' || text[4] != '-' || text[7] != '-' ||
        text[10] != 'T' || text[13] != ':' || text[16] != ':')
        return false;

    const auto number = [&](std::size_t start, std::size_t length) {
        int value = 0;
        for (const auto c : text.substr(start, length)) {
            if (!is_digit(c))
                return -1;
            value = value * 10 + c - '0';
        }
        return value;
    };
    const int year = number(0, 4), month = number(5, 2), day = number(8, 2);
    const int hour = number(11, 2), minute = number(14, 2), second = number(17, 2);
    if (year < 1 || month < 1 || month > 12 || day < 1 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
        return false;

    const int month_days[] = {31, leap_year(year) ? 29 : 28, 31, 30, 31, 30,
                              31, 31, 30, 31, 30, 31};
    if (day > month_days[month - 1])
        return false;
    int days = days_before_year(year) - days_before_year(1970) + day - 1;
    for (int i = 0; i < month - 1; ++i)
        days += month_days[i];

    int nanos = 0;
    if (text.size() != 20) {
        if (text[19] != '.' || text.size() < 22 || text.size() > 30)
            return false;
        const auto digits = text.size() - 21;
        nanos = number(20, digits);
        if (nanos < 0)
            return false;
        for (auto i = digits; i < 9; ++i)
            nanos *= 10;
    }
    time = {static_cast<std::int64_t>(days) * 86400 + hour * 3600 + minute * 60 + second, nanos};
    return true;
}

constexpr SlasconeStatus check_date_range(UtcTime start, UtcTime expiration, UtcTime now)
{
    if (now < start)
        return SLASCONE_LICENSE_NOT_YET_VALID;
    if (expiration < now)
        return SLASCONE_LICENSE_EXPIRED;
    return SLASCONE_OK;
}

static_assert(check_date_range({1, 7}, {2, 7}, {1, 6}) == SLASCONE_LICENSE_NOT_YET_VALID);
static_assert(check_date_range({1, 7}, {2, 7}, {1, 7}) == SLASCONE_OK);
static_assert(check_date_range({1, 7}, {2, 7}, {2, 7}) == SLASCONE_OK);
static_assert(check_date_range({1, 7}, {2, 7}, {2, 8}) == SLASCONE_LICENSE_EXPIRED);

SlasconeStatus check_dates(xmlNodePtr license)
{
    std::string start_text, expiration_text;
    UtcTime start, expiration;
    if (!read_field(license, "start_date_utc", start_text) ||
        !read_field(license, "expiration_date_utc", expiration_text) ||
        !parse_utc(start_text, start) || !parse_utc(expiration_text, expiration) ||
        expiration < start)
        return SLASCONE_INVALID_LICENSE_DATES;

    using namespace std::chrono;
    const auto now = system_clock::now().time_since_epoch();
    const auto whole_seconds = floor<seconds>(now);
    const UtcTime utc_now{whole_seconds.count(), static_cast<int>(
        duration_cast<nanoseconds>(now - whole_seconds).count())};
    return check_date_range(start, expiration, utc_now);
}

}

SlasconeStatus check_license(const char *license_path, const char *activation_path,
                             const std::string &client_id, const std::string &release,
                             const std::string &pem)
{
    VerificationKey key(nullptr, xmlSecKeyDestroy);
    auto status = load_verification_key(pem, key);
    if (status != SLASCONE_OK)
        return status;

    XmlDocument license(nullptr, xmlFreeDoc), activation(nullptr, xmlFreeDoc);
    status = read_signed_xml(license_path, key, license);
    if (status != SLASCONE_OK)
        return status;
    const auto license_root = xmlDocGetRootElement(license.get());
    std::string license_key, revision;
    if (!matches(license_root, "slascone_license_file") ||
        !read_field(license_root, "license_key", license_key) || license_key.empty() ||
        !read_field(license_root, "revision_id", revision))
        return SLASCONE_INVALID_XML;

    auto client_source = license_root;
    if (activation_path && *activation_path) {
        status = read_signed_xml(activation_path, key, activation);
        if (status != SLASCONE_OK)
            return status;
        client_source = xmlDocGetRootElement(activation.get());
        std::string activation_key, activation_revision;
        if (!matches(client_source, "slascone_activation_file") ||
            !read_field(client_source, "license_key", activation_key) || activation_key.empty() ||
            !read_field(client_source, "revision_id", activation_revision))
            return SLASCONE_INVALID_XML;
        if (!same_id(license_key, activation_key))
            return SLASCONE_LICENSE_KEY_MISMATCH;
        if (!same_id(revision, activation_revision))
            return SLASCONE_REVISION_MISMATCH;
    }

    std::string bound_client;
    if (!read_field(client_source, "client_id", bound_client))
        return SLASCONE_INVALID_XML;
    if (bound_client.empty() || !same_id(client_id, bound_client))
        return SLASCONE_CLIENT_ID_MISMATCH;

    status = check_release(license_root, release);
    return status == SLASCONE_OK ? check_dates(license_root) : status;
}

}
