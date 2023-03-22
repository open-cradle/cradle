#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/client/proxy_impl.h>

namespace cradle {

rpclib_client::rpclib_client(service_config const& config)
    : pimpl_{std::make_unique<rpclib_client_impl>(config)}
{
}

rpclib_client::~rpclib_client()
{
}

std::string
rpclib_client::name() const
{
    return "rpclib";
}

cppcoro::task<serialized_result>
rpclib_client::resolve_request(remote_context_intf& ctx, std::string seri_req)
{
    return pimpl_->resolve_request(ctx, std::move(seri_req));
}

void
rpclib_client::mock_http(std::string const& response_body)
{
    pimpl_->mock_http(response_body);
}

// Note is blocking
std::string
rpclib_client::ping()
{
    return pimpl_->ping();
}

} // namespace cradle
