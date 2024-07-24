#include <stdexcept>
#include <string>

#include <cppcoro/sync_wait.hpp>
#include <fmt/format.h>
#include <msgpack.hpp>
#include <spdlog/spdlog.h>

#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/encodings/msgpack_dump.h>
#include <cradle/inner/requests/types.h>
#include <cradle/inner/utilities/logging.h>
#include <cradle/rpclib/cli/types.h>
#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/common/common.h>
#include <cradle/rpclib/common/config.h>

namespace cradle {

static service_config_map
create_config_map(cli_options const& options)
{
    service_config_map config_map;
    config_map[rpclib_config_keys::EXPECT_SERVER] = true;
    config_map[rpclib_config_keys::PORT_NUMBER] = options.port;
    return config_map;
}

void
cmd_show(cli_options const& options)
{
    auto const remote_id = get_remote_id(options);
    initialize_logging(options.log_level, options.log_level_set, "cli ");
    auto my_logger = create_logger("cli");
    service_config config{create_config_map(options)};
    rpclib_client client{config, nullptr, my_logger};

    auto status = client.get_async_status(remote_id);
    fmt::print("id {}: status {}\n", remote_id, to_string(status));

    auto sub_specs = client.get_sub_contexts(remote_id);
    auto num_sub_specs = sub_specs.size();
    for (decltype(num_sub_specs) i = 0; i < num_sub_specs; ++i)
    {
        auto const& spec = sub_specs[i];
        auto const& [spec_id, is_req] = spec;
        fmt::print(
            "sub [{}]: id {} ({})\n", i, spec_id, is_req ? "REQ" : "VAL");
    }

    if (status == async_status::ERROR)
    {
        fmt::print("error: {}\n", client.get_async_error_message(remote_id));
    }

    try
    {
        auto essentials = client.get_essentials(remote_id);
        fmt::print("uuid {}\n", essentials.uuid_str);
        if (essentials.title)
        {
            fmt::print("title {}\n", *essentials.title);
        }
    }
    catch (remote_error const& e)
    {
        my_logger->warn("No essentials for id {}: {}", remote_id, e.what());
    }

    if (status == async_status::FINISHED)
    {
        try
        {
            auto result = client.get_async_response(remote_id);
            auto x = result.value();
            msgpack::object_handle oh = msgpack::unpack(
                reinterpret_cast<char const*>(x.data()), x.size());
            msgpack::object obj = oh.get();
            fmt::print("result: ");
            dump_msgpack_object(obj);
        }
        catch (remote_error const& e)
        {
            // No root ctx probably
            my_logger->warn("No result for id {}: {}", remote_id, e.what());
        }
    }
}

} // namespace cradle
