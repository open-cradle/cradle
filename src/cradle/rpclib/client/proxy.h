#ifndef CRADLE_RPCLIB_CLIENT_PROXY_H
#define CRADLE_RPCLIB_CLIENT_PROXY_H

#include <memory>

#include <cradle/inner/remote/proxy.h>
#include <cradle/rpclib/common/port.h>

namespace cradle {

class rpclib_client_impl;
class ephemeral_port_owner;

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
    // If port_owner is set, the client and server run in contained mode, where
    // the server will execute a single function at a time, without any form
    // of caching. In that mode, the server listens on an ephemeral port, which
    // must be allocated from, and returned to, *port_owner.
    // In non-contained mode, the port number is fixed, depending on whether
    // the server runs in testing mode.
    rpclib_client(
        service_config const& config,
        ephemeral_port_owner* port_owner,
        std::shared_ptr<spdlog::logger> logger
        = std::shared_ptr<spdlog::logger>());

    ~rpclib_client();

    rpclib_port_t
    get_port() const;

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

    request_essentials
    get_essentials(async_id root_aid) override;

    void
    request_cancellation(async_id aid) override;

    void
    finish_async(async_id root_aid) override;

    tasklet_info_list
    get_tasklet_infos(bool include_finished) override;

    void
    load_shared_library(std::string dir_path, std::string dll_name) override;

    void
    unload_shared_library(std::string dll_name) override;

    void
    mock_http(std::string const& response_body) override;

    void
    clear_unused_mem_cache_entries() override;

    void
    release_cache_record_lock(remote_cache_record_id record_id) override;

    int
    get_num_contained_calls() const override;

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
