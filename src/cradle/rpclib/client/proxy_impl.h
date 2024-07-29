#ifndef CRADLE_RPCLIB_CLIENT_PROXY_IMPL_H
#define CRADLE_RPCLIB_CLIENT_PROXY_IMPL_H

#include <memory>
#include <mutex>
#include <optional>
#include <set>
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
        service_config const& config,
        ephemeral_port_owner* port_owner,
        std::shared_ptr<spdlog::logger> logger);

    ~rpclib_client_impl();

    rpclib_port_t
    get_port() const
    {
        return port_;
    }

 private:
    friend class rpclib_client;
    friend class rpclib_deserialization_observer;

    // All timeouts given in milliseconds

    // Timeout for establishing a connection, and timeout for detecting a
    // running server.
    // Establishing a connection on Windows tends to be slow.
#if defined(_MSC_VER)
    static inline constexpr int connection_timeout{10000};
    static inline constexpr int detect_server_timeout{30000};
#else
    static inline constexpr int connection_timeout{1000};
    static inline constexpr int detect_server_timeout{5000};
#endif

    // Timeout for RPC calls that should be fast
    static inline constexpr int default_timeout{2000};
    // Timeout for receiving an async response, which could be GB's of data
    static inline constexpr int get_async_response_timeout{20000};
    // Timeout for loading a shared library
    static inline constexpr int load_dll_timeout{10000};

    std::string
    ping(int timeout);

    void
    verify_rpclib_protocol(std::string const& server_rpclib_protocol);

    void
    ack_response(uint32_t pool_id);

    bool
    server_is_running();
    void
    wait_until_server_running();
    void
    ensure_server();
    void
    start_server();
    void
    stop_server();

    template<typename... Params>
    RPCLIB_MSGPACK::object_handle
    do_rpc_call(std::string const& func_name, int timeout, Params&&... params);

    template<typename... Params>
    void
    do_rpc_async_call(std::string const& func_name, Params&&... params);

    serialized_result
    make_serialized_result(rpclib_response const& response);

    ephemeral_port_owner* const port_owner_;
    std::shared_ptr<spdlog::logger> logger_;
    bool const testing_{};
    bool const contained_{};
    bool const expect_server_{};
    std::optional<std::string> const deploy_dir_;
    rpclib_port_t const port_{};
    std::optional<std::string> secondary_cache_factory_;

    // On Windows, localhost and 127.0.0.1 are not the same:
    // https://stackoverflow.com/questions/68957411/winsock-connect-is-slow
    static inline std::string const localhost_{"127.0.0.1"};
    std::unique_ptr<rpc::client> rpc_client_;

    // Server subprocess. A real (not contained) server process is put in a new
    // process group (group_) so that any (contained) subprocesses terminate
    // when the server is terminated. For the same reason, contained processes
    // are _not_ put in a new group; group_ is unused for them.
    boost::process::group group_;
    boost::process::child child_;

    std::mutex loaded_dlls_mutex_;
    std::set<std::string> loaded_dlls_;
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
