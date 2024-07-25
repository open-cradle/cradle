#ifndef CRADLE_RPCLIB_CLI_TYPES_H
#define CRADLE_RPCLIB_CLI_TYPES_H

#include <optional>
#include <stdexcept>
#include <string>

#include <boost/program_options.hpp>

#include <cradle/rpclib/common/common.h>

namespace cradle {

struct cli_options
{
    std::string command;
    std::string log_level{"critical"};
    bool log_level_set{false};
    rpclib_port_t port{RPCLIB_PORT_PRODUCTION};
    std::optional<int> remote_id;
    std::optional<std::string> storage_name;
    std::optional<std::string> key;
    std::optional<std::string> domain_name;
    std::optional<int> arg0;
};

int
get_remote_id(cli_options const& options);

std::string
get_storage_name(cli_options const& options);

std::string
get_key(cli_options const& options);

std::string
get_domain_name(cli_options const& options);

int
get_arg0(cli_options const& options);

class command_line_error : public boost::program_options::error
{
    using boost::program_options::error::error;
};

} // namespace cradle

#endif
