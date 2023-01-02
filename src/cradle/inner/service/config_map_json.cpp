#include <mutex>
#include <stdexcept>

#include <simdjson.h>

#include <cradle/inner/service/config_map_json.h>

namespace cradle {

namespace {

struct json_config_error : public std::logic_error
{
    using logic_error::logic_error;
};

config_value
parse_json_value(simdjson::dom::element const& json)
{
    switch (json.type())
    {
        case simdjson::dom::element_type::BOOL:
            return config_value{bool(json)};
        case simdjson::dom::element_type::INT64:
            return config_value{static_cast<std::size_t>(int64_t(json))};
        case simdjson::dom::element_type::UINT64:
            return config_value{static_cast<std::size_t>(uint64_t(json))};
        case simdjson::dom::element_type::STRING:
            return config_value{std::string{std::string_view(json)}};
        default:
            break;
    }
    throw json_config_error("JSON value has unsupported type");
}

void
parse_json_object(
    simdjson::dom::object const& obj,
    std::string const& key_prefix,
    service_config_map& result)
{
    for (auto const& kv : obj)
    {
        std::string key{key_prefix + std::string(kv.key)};
        auto const& value{kv.value};
        if (value.type() == simdjson::dom::element_type::OBJECT)
        {
            parse_json_object(value, key + '/', result);
        }
        else
        {
            result[key] = parse_json_value(value);
        }
    }
}

service_config_map
parse_json_doc(simdjson::dom::element const& json)
{
    service_config_map result;
    parse_json_object(json, "", result);
    return result;
}

} // namespace

// The JSON should be an object like
//
// {
//     "disk_cache": {
//         "directory": "/var/cache/cradle",
//         "size_limit": 6000000000
//     },
//     "open": true
// }
//
// Values can be unsigned integers, booleans or strings.
// Errors lead to an exception being thrown.
service_config_map
read_config_map_from_json(std::string const& json_text)
{
    static simdjson::dom::parser the_parser;
    static std::mutex the_mutex;

    std::scoped_lock<std::mutex> guard(the_mutex);

    simdjson::dom::element doc
        = the_parser.parse(json_text.c_str(), json_text.size());
    return parse_json_doc(doc);
}

} // namespace cradle
