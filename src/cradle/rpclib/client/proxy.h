#ifndef CRADLE_RPCLIB_CLIENT_PROXY_H
#define CRADLE_RPCLIB_CLIENT_PROXY_H

#include <memory>
#include <stdexcept>

#include <fmt/format.h>

#include <cradle/inner/remote/proxy.h>
#include <cradle/inner/service/config.h>
#include <cradle/inner/service/seri_result.h>

namespace cradle {

// Clients shouldn't refer to rpc::rpc_error as that would #include the msgpack
// inside the rpclib library
class rpclib_error : public std::logic_error
{
 public:
    rpclib_error(std::string const& what) : std::logic_error(what)
    {
    }

    rpclib_error(std::string const& what, std::string const& msg)
        : std::logic_error(fmt::format("{}: {}", what, msg))
    {
    }
};

class rpclib_client_impl;

class rpclib_client : public remote_proxy
{
 public:
    rpclib_client(service_config const& config);

    ~rpclib_client();

    std::string
    name() const override;

    // Throws rpclib_error
    cppcoro::task<serialized_result>
    resolve_request(remote_context_intf& ctx, std::string seri_req) override;

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
    std::unique_ptr<rpclib_client_impl> pimpl_;
};

} // namespace cradle

#endif
