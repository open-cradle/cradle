#ifndef CRADLE_RPCLIB_CLIENT_PROXY_H
#define CRADLE_RPCLIB_CLIENT_PROXY_H

#include <memory>
#include <stdexcept>

#include <fmt/format.h>

#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/service/config.h>
#include <cradle/inner/service/seri_result.h>

namespace cradle {

class rpclib_client_impl;

// Coroutines should throw remote_error on error.
// The rpclib library throws rpc::rpc_error so these should be translated to
// remote_error.
// Clients cannot refer to rpc::rpc_error anyway as that would #include the
// msgpack implementation inside the rpclib library, conflicting with the main
// one.
class rpclib_client : public remote_proxy
{
 public:
    rpclib_client(service_config const& config);

    ~rpclib_client();

    std::string
    name() const override;

    spdlog::logger&
    get_logger() override;

    cppcoro::static_thread_pool&
    get_coro_thread_pool() override
    {
        return coro_thread_pool_;
    }

    cppcoro::task<serialized_result>
    resolve_sync(
        remote_context_intf& ctx,
        std::string domain_name,
        std::string seri_req) override;

    cppcoro::task<async_id>
    submit_async(
        remote_context_intf& ctx,
        std::string domain_name,
        std::string seri_req) override;

    cppcoro::task<remote_context_spec_list>
    get_sub_contexts(async_id aid) override;

    cppcoro::task<async_status>
    get_async_status(async_id aid) override;

    cppcoro::task<std::string>
    get_async_error_message(async_id aid) override;

    cppcoro::task<serialized_result>
    get_async_response(async_id root_aid) override;

    cppcoro::task<void>
    request_cancellation(async_id aid) override;

    cppcoro::task<void>
    finish_async(async_id root_aid) override;

    // Instructs the RPC server to mock all HTTP requests, returning a 200
    // response with response_body for each.
    // Blocking RPC call.
    void
    mock_http(std::string const& response_body);

    // Tests if the rpclib server is running, throws rpc::system_error if not.
    // Returns the Git version identifying the code base.
    // Blocking RPC call.
    std::string
    ping();

    // Intended for test purposes only
    rpclib_client_impl&
    pimpl()
    {
        return *pimpl_;
    }

 private:
    cppcoro::static_thread_pool coro_thread_pool_;
    std::unique_ptr<rpclib_client_impl> pimpl_;
};

} // namespace cradle

#endif
