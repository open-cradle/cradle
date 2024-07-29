#include <fmt/format.h>

#include <cradle/rpclib/cli/cmd_common.h>
#include <cradle/rpclib/cli/cmd_submit.h>
#include <cradle/rpclib/cli/types.h>
#include <cradle/rpclib/client/proxy.h>

namespace cradle {

void
cmd_submit(cli_options const& options)
{
    auto storage_name = get_storage_name(options);
    auto key = get_key(options);
    get_domain_name(options);

    auto logger{create_logger(options)};
    service_config config{create_config_map(options)};
    rpclib_client client{config, nullptr, logger};

    auto remote_id = client.submit_stored(
        std::move(config), std::move(storage_name), std::move(key));
    fmt::print("Submitted: remote_id {}\n", remote_id);
}

} // namespace cradle
