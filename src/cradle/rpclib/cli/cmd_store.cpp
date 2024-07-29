#include <fmt/format.h>

#include <cradle/inner/core/get_unique_string.h>
#include <cradle/inner/requests/serialization.h>
#include <cradle/inner/service/config.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/rpclib/cli/cmd_common.h>
#include <cradle/rpclib/cli/cmd_store.h>
#include <cradle/rpclib/cli/types.h>
#include <cradle/rpclib/client/proxy.h>

namespace cradle {

void
cmd_store(cli_options const& options)
{
    auto storage_name = get_storage_name(options);
    auto loops = 3;
    auto delay0 = 5;
    auto delay1 = get_arg0(options);

    auto logger{create_logger(options)};
    service_config config{create_config_map(options)};
    rpclib_client client{config, nullptr, logger};

    constexpr auto level = caching_level_type::memory;
    auto req{rq_cancellable_coro<level>(
        rq_cancellable_coro<level>(loops, delay0),
        rq_cancellable_coro<level>(loops, delay1))};
    std::string key{get_unique_string(*req.get_captured_id())};
    client.store_request(storage_name, key, serialize_request(req));
    fmt::print("request stored under {}\n", key);
}

} // namespace cradle
