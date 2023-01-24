#ifndef CRADLE_RPCLIB_CLIENT_PROXY_IMPL_H
#define CRADLE_RPCLIB_CLIENT_PROXY_IMPL_H

#include <memory>
#include <string>
#include <tuple>

#include <boost/process.hpp>
#include <cppcoro/static_thread_pool.hpp>
#include <cppcoro/task.hpp>
#include <rpc/client.h>
#include <spdlog/spdlog.h>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/config.h>
#include <cradle/inner/service/seri_result.h>

namespace cradle {

class rpclib_client_impl
{
 public:
    rpclib_client_impl(service_config const& config);

    ~rpclib_client_impl();

    cppcoro::task<serialized_result>
    resolve_request(remote_context_intf& ctx, std::string seri_req);

    void
    mock_http(std::string const& response_body);

    std::string
    ping();

    void
    ack_response(uint32_t pool_id);

    void
    verify_git_version(std::string const& server_git_version);

 private:
    inline static std::shared_ptr<spdlog::logger> logger_;
    uint16_t port_;
    cppcoro::static_thread_pool coro_thread_pool_;

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
