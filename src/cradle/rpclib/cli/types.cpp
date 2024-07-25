#include <optional>

#include <fmt/format.h>

#include <cradle/rpclib/cli/types.h>

namespace cradle {

namespace {

template<typename T>
T
get_option(std::optional<T> const& opt_value, std::string const& option_name)
{
    if (!opt_value)
    {
        throw command_line_error{fmt::format("missing --{}", option_name)};
    }
    return *opt_value;
}

} // namespace

int
get_remote_id(cli_options const& options)
{
    return get_option(options.remote_id, "id");
}

std::string
get_storage_name(cli_options const& options)
{
    return get_option(options.storage_name, "storage");
}

std::string
get_key(cli_options const& options)
{
    return get_option(options.key, "key");
}

std::string
get_domain_name(cli_options const& options)
{
    return get_option(options.domain_name, "domain");
}

int
get_arg0(cli_options const& options)
{
    return get_option(options.arg0, "arg0");
}

} // namespace cradle
