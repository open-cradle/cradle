#ifndef CRADLE_RPCLIB_CLIENT_PROXY_IMPL_H
#define CRADLE_RPCLIB_CLIENT_PROXY_IMPL_H

#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include <boost/process.hpp>
#include <rpc/client.h>
#include <spdlog/spdlog.h>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/resolve/seri_result.h>
#include <cradle/inner/service/config.h>
#include <cradle/rpclib/common/common.h>

namespace cradle {

// This class offers roughly the same functions as rpclib_client, but these
// functions are blocking, and not coroutines.
class rpclib_client_impl
{
 public:
    rpclib_client_impl(service_config const& config);

    ~rpclib_client_impl();

    // Set a logger to be used by rpclib_client_impl instances.
    static void
    set_logger(std::shared_ptr<spdlog::logger> logger)
    {
        logger_ = std::move(logger);
    }

    spdlog::logger&
    get_logger()
    {
        return *logger_;
    }

    serialized_result
    resolve_sync(service_config config, std::string seri_req);

    async_id
    submit_async(service_config config, std::string seri_req);

    remote_context_spec_list
    get_sub_contexts(async_id aid);

    async_status
    get_async_status(async_id aid);

    std::string
    get_async_error_message(async_id aid);

    serialized_result
    get_async_response(async_id root_aid);

    void
    request_cancellation(async_id aid);

    void
    finish_async(async_id root_aid);

    tasklet_info_tuple_list
    get_tasklet_infos(bool include_finished);

    void
    mock_http(std::string const& response_body);

    std::string
    ping();

    void
    ack_response(uint32_t pool_id);

    void
    verify_rpclib_protocol(std::string const& server_rpclib_protocol);

 private:
    inline static std::shared_ptr<spdlog::logger> logger_;
    bool testing_;
    std::optional<std::string> deploy_dir_;
    uint16_t port_;
    std::string secondary_cache_factory_;

    // On Windows, localhost and 127.0.0.1 are not the same:
    // https://stackoverflow.com/questions/68957411/winsock-connect-is-slow
    std::string localhost_{"127.0.0.1"};
    std::unique_ptr<rpc::client> rpc_client_;

    // Server subprocess
    boost::process::group group_;
    boost::process::child child_;

    bool
    server_is_running();
    void
    wait_until_server_running();
    void
    start_server();
    void
    stop_server();

    template<typename... Args>
    RPCLIB_MSGPACK::object_handle
    do_rpc_call(std::string const& func_name, Args... args);

    template<typename... Args>
    void
    do_rpc_async_call(std::string const& func_name, Args... args);

    serialized_result
    make_serialized_result(rpclib_response const& response);
};

class rpclib_deserialization_observer : public deserialization_observer
{
 public:
    rpclib_deserialization_observer(
        rpclib_client_impl& client, uint32_t pool_id);

    ~rpclib_deserialization_observer() = default;

    void
    on_deserialized() override;

 private:
    rpclib_client_impl& client_;
    uint32_t pool_id_;
};

} // namespace cradle

#endif
