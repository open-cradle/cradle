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

class rpclib_client_impl
{
 public:
    rpclib_client_impl(
        service_config const& config, std::shared_ptr<spdlog::logger> logger);

    ~rpclib_client_impl();

 private:
    friend class rpclib_client;
    friend class rpclib_deserialization_observer;

    std::string
    ping();

    void
    verify_rpclib_protocol(std::string const& server_rpclib_protocol);

    void
    ack_response(uint32_t pool_id);

    bool
    server_is_running();
    void
    wait_until_server_running();
    void
    start_server();
    void
    stop_server();

    template<typename... Params>
    RPCLIB_MSGPACK::object_handle
    do_rpc_call(std::string const& func_name, Params&&... params);

    template<typename... Params>
    void
    do_rpc_async_call(std::string const& func_name, Params&&... params);

    serialized_result
    make_serialized_result(rpclib_response const& response);

    std::shared_ptr<spdlog::logger> logger_;
    bool testing_{};
    std::optional<std::string> deploy_dir_;
    uint16_t port_{};
    std::string secondary_cache_factory_;

    // On Windows, localhost and 127.0.0.1 are not the same:
    // https://stackoverflow.com/questions/68957411/winsock-connect-is-slow
    static inline std::string const localhost_{"127.0.0.1"};
    std::unique_ptr<rpc::client> rpc_client_;

    // Server subprocess
    boost::process::group group_;
    boost::process::child child_;
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
