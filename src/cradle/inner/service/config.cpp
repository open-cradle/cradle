#include <cradle/inner/service/config.h>

#include <sstream>

namespace cradle {

namespace {

template<typename T>
T
convert_value(config_value const& value, std::string const& key)
{
    T const* pt{std::get_if<T>(&value)};
    if (!pt)
    {
        std::ostringstream os;
        os << "Bad type for config \"" << key << "\"";
        throw config_error(os.str());
    }
    return *pt;
}

} // namespace

service_config::service_config() = default;

service_config::service_config(service_config_map const& config_map)
    : config_map_{config_map}
{
}

bool
service_config::contains(std::string const& key) const
{
    return config_map_.find(key) != config_map_.end();
}

template<typename T>
std::optional<T>
service_config::get_optional(std::string const& key) const
{
    auto it = config_map_.find(key);
    if (it == config_map_.end())
    {
        return std::optional<T>();
    }
    return convert_value<T>(it->second, key);
}

template std::optional<std::string>
service_config::get_optional(std::string const& key) const;
template std::optional<size_t>
service_config::get_optional(std::string const& key) const;
template std::optional<bool>
service_config::get_optional(std::string const& key) const;

template<typename T>
T
service_config::get_mandatory(std::string const& key) const
{
    auto it = config_map_.find(key);
    if (it == config_map_.end())
    {
        std::ostringstream os;
        os << "Missing mandatory config \"" << key << "\"";
        throw config_error(os.str());
    }
    return convert_value<T>(it->second, key);
}

template std::string
service_config::get_mandatory(std::string const& key) const;
template size_t
service_config::get_mandatory(std::string const& key) const;
template bool
service_config::get_mandatory(std::string const& key) const;

template<typename T>
T
service_config::get_value_or_default(
    std::string const& key, T const& default_value) const
{
    auto it = config_map_.find(key);
    if (it == config_map_.end())
    {
        return default_value;
    }
    return convert_value<T>(it->second, key);
}

template std::string
service_config::get_value_or_default(
    std::string const& key, std::string const& default_value) const;
template size_t
service_config::get_value_or_default(
    std::string const& key, size_t const& default_value) const;
template bool
service_config::get_value_or_default(
    std::string const& key, bool const& default_value) const;

} // namespace cradle
