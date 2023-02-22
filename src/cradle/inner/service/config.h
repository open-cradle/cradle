#ifndef CRADLE_INNER_SERVICE_CONFIG_H
#define CRADLE_INNER_SERVICE_CONFIG_H

#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

namespace cradle {

struct config_error : public std::logic_error
{
    using logic_error::logic_error;
};

using config_value = std::variant<std::string, std::size_t, bool>;

// A key-value map specifying a configuration
using service_config_map = std::map<std::string, config_value>;

// Configuration for one or more service layers.
// A key-value map where values are strings, (unsigned) numbers or booleans:
// an open-ended format, where each layer will interpret the keys it
// understands, and pass on the map as-is to other layers.
// Each layer should specify somewhere what keys it understands.
class service_config
{
 public:
    service_config();

    service_config(service_config_map const& config_map);

    bool
    contains(std::string const& key) const;

    std::optional<std::string>
    get_optional_string(std::string const& key) const
    {
        return get_optional<std::string>(key);
    }

    std::string
    get_mandatory_string(std::string const& key) const
    {
        return get_mandatory<std::string>(key);
    }

    std::string
    get_string_or_default(
        std::string const& key, std::string const& default_value) const
    {
        return get_value_or_default<std::string>(key, default_value);
    }

    std::optional<size_t>
    get_optional_number(std::string const& key) const
    {
        return get_optional<size_t>(key);
    }

    size_t
    get_mandatory_number(std::string const& key) const
    {
        return get_mandatory<size_t>(key);
    }

    size_t
    get_number_or_default(std::string const& key, size_t default_value) const
    {
        return get_value_or_default<size_t>(key, default_value);
    }

    std::optional<bool>
    get_optional_bool(std::string const& key) const
    {
        return get_optional<bool>(key);
    }

    bool
    get_mandatory_bool(std::string const& key) const
    {
        return get_mandatory<bool>(key);
    }

    bool
    get_bool_or_default(std::string const& key, bool default_value) const
    {
        return get_value_or_default<bool>(key, default_value);
    }

 private:
    service_config_map config_map_;

    template<typename T>
    std::optional<T>
    get_optional(std::string const& key) const;

    template<typename T>
    T
    get_mandatory(std::string const& key) const;

    template<typename T>
    T
    get_value_or_default(std::string const& key, T const& default_value) const;
};

// Generic configuration keys
struct generic_config_keys
{
    // (Optional boolean)
    // true in testing (non-production) context
    inline static std::string const TESTING{"testing"};
};

} // namespace cradle

#endif
