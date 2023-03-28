#include <stdexcept>

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <toml++/toml.h>

#include <cradle/inner/service/config_map_toml.h>

namespace cradle {

namespace {

struct toml_config_error : public std::domain_error
{
    using domain_error::domain_error;
};

void
report_error(toml::key const& key, std::string const& config_key)
{
    auto source = key.source();
    std::string where;
    if (source.path)
    {
        where = fmt::format("{}:{}", *source.path, source.begin.line);
    }
    else
    {
        where = fmt::format("Line {}", source.begin.line);
    }
    spdlog::get("cradle")->error(
        fmt::format("{}: unsupported value for key {}", where, config_key));
}

void
handle_table(
    toml::table const& tbl,
    std::string const& key_prefix,
    service_config_map& result,
    bool& have_errors)
{
    tbl.for_each([&](toml::key const& key, auto&& val) {
        std::string config_key = key_prefix + std::string{key.str()};
        const toml::node& n = val;
        if constexpr (toml::is_string<decltype(val)>)
        {
            if (auto opt_str = n.value<std::string>())
            {
                result[config_key] = *opt_str;
                return;
            }
        }
        if constexpr (toml::is_integer<decltype(val)>)
        {
            if (auto opt_num = n.value<size_t>())
            {
                result[config_key] = *opt_num;
                return;
            }
        }
        if constexpr (toml::is_boolean<decltype(val)>)
        {
            if (auto opt_bool = n.value<bool>())
            {
                result[config_key] = *opt_bool;
                return;
            }
        }
        if constexpr (toml::is_table<decltype(val)>)
        {
            if (auto opt_table = val.as_table())
            {
                handle_table(
                    *opt_table, config_key + "/", result, have_errors);
                return;
            }
        }
        report_error(key, config_key);
        have_errors = true;
    });
}

service_config_map
handle_outer_table(toml::table const& tbl)
{
    service_config_map result;
    bool have_errors{false};
    handle_table(tbl, "", result, have_errors);
    if (have_errors)
    {
        throw toml_config_error{"Errors in TOML"};
    }
    return result;
}

} // namespace

service_config_map
read_config_map_from_toml(std::string const& toml_text)
{
    toml::table tbl = toml::parse(toml_text);
    return handle_outer_table(tbl);
}

service_config_map
read_config_map_from_toml_file(std::string const& path)
{
    toml::table tbl = toml::parse_file(path);
    return handle_outer_table(tbl);
}

} // namespace cradle
