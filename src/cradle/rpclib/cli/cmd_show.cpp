#include <fmt/format.h>
#include <msgpack.hpp>

#include <cradle/inner/core/fmt_format.h>
#include <cradle/inner/encodings/msgpack_dump.h>
#include <cradle/inner/requests/types.h>
#include <cradle/rpclib/cli/cmd_common.h>
#include <cradle/rpclib/cli/cmd_show.h>
#include <cradle/rpclib/cli/types.h>
#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/common/common.h>

namespace cradle {

void
cmd_show(cli_options const& options)
{
    auto const remote_id = get_remote_id(options);
    auto logger{create_logger(options)};
    service_config config{create_config_map(options)};
    rpclib_client client{config, nullptr, logger};

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

    if (status == async_status::FAILED)
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
        logger->warn("No essentials for id {}: {}", remote_id, e.what());
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
            logger->warn("No result for id {}: {}", remote_id, e.what());
        }
    }
}

} // namespace cradle
