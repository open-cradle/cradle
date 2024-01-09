#ifndef CRADLE_RPCLIB_CLIENT_PROXY_H
#define CRADLE_RPCLIB_CLIENT_PROXY_H

#include <memory>

#include <cradle/inner/remote/proxy.h>

namespace cradle {

class rpclib_client_impl;

// RPC calls should throw remote_error on error.
// The rpclib library throws rpc::rpc_error so these should be translated to
// remote_error.
// Clients cannot refer to rpc::rpc_error anyway as that would #include the
// msgpack implementation inside the rpclib library, conflicting with the main
// one.
// All RPC calls are blocking.
class rpclib_client : public remote_proxy
{
 public:
    rpclib_client(
        service_config const& config,
        std::shared_ptr<spdlog::logger> logger
        = std::shared_ptr<spdlog::logger>());

    ~rpclib_client();

    std::string
    name() const override;

    spdlog::logger&
    get_logger() override;

    serialized_result
    resolve_sync(service_config config, std::string seri_req) override;

    async_id
    submit_async(service_config config, std::string seri_req) override;

    remote_context_spec_list
    get_sub_contexts(async_id aid) override;

    async_status
    get_async_status(async_id aid) override;

    std::string
    get_async_error_message(async_id aid) override;

    serialized_result
    get_async_response(async_id root_aid) override;

    void
    request_cancellation(async_id aid) override;

    void
    finish_async(async_id root_aid) override;

    tasklet_info_tuple_list
    get_tasklet_infos(bool include_finished) override;

    void
    load_shared_library(std::string dir_path, std::string dll_name) override;

    void
    unload_shared_library(std::string dll_name) override;

    void
    mock_http(std::string const& response_body) override;

    // Tests if the rpclib server is running, throws rpc::system_error if not.
    // Returns a compatibility identifier.
    std::string
    ping();

    void
    verify_rpclib_protocol(std::string const& server_rpclib_protocol);

 private:
    std::unique_ptr<rpclib_client_impl> pimpl_;
};

} // namespace cradle

#endif
