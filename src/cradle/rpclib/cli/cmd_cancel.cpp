#include <cradle/rpclib/cli/cmd_cancel.h>
#include <cradle/rpclib/cli/cmd_common.h>
#include <cradle/rpclib/cli/types.h>
#include <cradle/rpclib/client/proxy.h>

namespace cradle {

void
cmd_cancel(cli_options const& options)
{
    auto const remote_id = get_remote_id(options);
    auto logger{create_logger(options)};
    service_config config{create_config_map(options)};
    rpclib_client client{config, nullptr, logger};

    client.request_cancellation(remote_id);
}

} // namespace cradle
