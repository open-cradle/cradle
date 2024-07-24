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
};

int
get_remote_id(cli_options const& options);

class command_line_error : public boost::program_options::error
{
    using boost::program_options::error::error;
};

} // namespace cradle

#endif
