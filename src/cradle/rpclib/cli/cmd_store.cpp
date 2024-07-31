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

namespace {

template<Request Req>
std::string
store_request(cli_options const& options, Req const& req)
{
    auto logger{create_logger(options)};
    service_config config{create_config_map(options)};
    rpclib_client client{config, nullptr, logger};

    std::string key{get_unique_string(*req.get_captured_id())};
    auto storage_name = get_storage_name(options);
    client.store_request(storage_name, key, serialize_request(req));
    return key;
}

} // namespace

void
cmd_store(cli_options const& options)
{
    auto loops = 3;
    auto delay0 = 5;
    auto delay1 = get_arg0(options);
    constexpr auto level = caching_level_type::memory;

    std::string key;
    if (options.proxy_flag)
    {
        auto req{rq_cancellable_proxy<level>(
            rq_cancellable_proxy<level>(loops, delay0),
            rq_cancellable_proxy<level>(loops, delay1))};
        key = store_request(options, req);
    }
    else
    {
        auto req{rq_cancellable_coro<level>(
            rq_cancellable_coro<level>(loops, delay0),
            rq_cancellable_coro<level>(loops, delay1))};
        key = store_request(options, req);
    }
    fmt::print("request stored under {}\n", key);
}

} // namespace cradle
