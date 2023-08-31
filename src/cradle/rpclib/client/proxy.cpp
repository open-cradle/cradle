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

spdlog::logger&
rpclib_client::get_logger()
{
    return pimpl_->get_logger();
}

serialized_result
rpclib_client::resolve_sync(service_config config, std::string seri_req)
{
    return pimpl_->resolve_sync(std::move(config), std::move(seri_req));
}

async_id
rpclib_client::submit_async(service_config config, std::string seri_req)
{
    return pimpl_->submit_async(std::move(config), std::move(seri_req));
}

remote_context_spec_list
rpclib_client::get_sub_contexts(async_id aid)
{
    return pimpl_->get_sub_contexts(aid);
}

async_status
rpclib_client::get_async_status(async_id aid)
{
    return pimpl_->get_async_status(aid);
}

std::string
rpclib_client::get_async_error_message(async_id aid)
{
    return pimpl_->get_async_error_message(aid);
}

serialized_result
rpclib_client::get_async_response(async_id root_aid)
{
    return pimpl_->get_async_response(root_aid);
}

void
rpclib_client::request_cancellation(async_id aid)
{
    return pimpl_->request_cancellation(aid);
}

void
rpclib_client::finish_async(async_id root_aid)
{
    return pimpl_->finish_async(root_aid);
}

tasklet_info_tuple_list
rpclib_client::get_tasklet_infos(bool include_finished)
{
    return pimpl_->get_tasklet_infos(include_finished);
}

void
rpclib_client::load_shared_library(std::string dir_path, std::string dll_name)
{
    return pimpl_->load_shared_library(
        std::move(dir_path), std::move(dll_name));
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
