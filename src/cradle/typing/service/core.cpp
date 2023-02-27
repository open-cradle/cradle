#include <cradle/typing/service/core.h>
#include <cradle/typing/service/core_impl.h>

namespace cradle {

service_core::service_core() : inner_resources()
{
}

service_core::service_core(service_config const& config) : inner_resources()
{
    initialize(config);
}

service_core::~service_core() = default;

void
service_core::initialize(service_config const& config)
{
    inner_initialize(config);
    impl_.reset(new service_core_impl());
}

cppcoro::static_thread_pool&
service_core::get_local_compute_pool_for_image(
    std::pair<std::string, thinknode_provider_image_info> const& tag)
{
    return impl_->get_local_compute_pool_for_image(tag);
}

cppcoro::static_thread_pool&
service_core_impl::get_local_compute_pool_for_image(
    std::pair<std::string, thinknode_provider_image_info> const& tag)
{
    return local_compute_pool_.try_emplace(tag, 4).first->second;
}

} // namespace cradle
