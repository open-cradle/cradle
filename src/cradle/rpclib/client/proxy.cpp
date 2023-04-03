#include <cradle/rpclib/client/proxy.h>
#include <cradle/rpclib/client/proxy_impl.h>

namespace cradle {

rpclib_client::rpclib_client(service_config const& config)
    : coro_thread_pool_{cppcoro::static_thread_pool(
        static_cast<uint32_t>(config.get_number_or_default(
            rpclib_config_keys::CLIENT_CONCURRENCY, 22)))},
      pimpl_{std::make_unique<rpclib_client_impl>(config)}
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

spdlog::logger&
rpclib_client::get_logger()
{
    return pimpl_->get_logger();
}

cppcoro::task<serialized_result>
rpclib_client::resolve_sync(
    remote_context_intf& ctx, std::string domain_name, std::string seri_req)
{
    co_await coro_thread_pool_.schedule();
    co_return pimpl_->resolve_sync(
        std::move(domain_name), std::move(seri_req));
}

cppcoro::task<async_id>
rpclib_client::submit_async(std::string domain_name, std::string seri_req)
{
    co_await coro_thread_pool_.schedule();
    co_return pimpl_->submit_async(
        std::move(domain_name), std::move(seri_req));
}

cppcoro::task<remote_context_spec_list>
rpclib_client::get_sub_contexts(async_id aid)
{
    co_await coro_thread_pool_.schedule();
    co_return pimpl_->get_sub_contexts(aid);
}

cppcoro::task<async_status>
rpclib_client::get_async_status(async_id aid)
{
    co_await coro_thread_pool_.schedule();
    co_return pimpl_->get_async_status(aid);
}

cppcoro::task<serialized_result>
rpclib_client::get_async_response(async_id root_aid)
{
    co_await coro_thread_pool_.schedule();
    co_return pimpl_->get_async_response(root_aid);
}

cppcoro::task<void>
rpclib_client::request_cancellation(async_id aid)
{
    co_await coro_thread_pool_.schedule();
    co_return pimpl_->request_cancellation(aid);
}

cppcoro::task<void>
rpclib_client::finish_async(async_id root_aid)
{
    co_await coro_thread_pool_.schedule();
    co_return pimpl_->finish_async(root_aid);
}

void
rpclib_client::mock_http(std::string const& response_body)
{
    pimpl_->mock_http(response_body);
}

std::string
rpclib_client::ping()
{
    return pimpl_->ping();
}

} // namespace cradle
