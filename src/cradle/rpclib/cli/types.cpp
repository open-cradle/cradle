#include <cradle/rpclib/cli/types.h>

namespace cradle {

int
get_remote_id(cli_options const& options)
{
    if (!options.remote_id)
    {
        throw command_line_error{"missing --id"};
    }
    return *options.remote_id;
}

} // namespace cradle
