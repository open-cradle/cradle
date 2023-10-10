#include <cradle/thinknode/service/core.h>
#include <cradle/thinknode/service/core_impl.h>

namespace cradle {

service_core::service_core(service_config const& config)
    : inner_resources(config), impl_{std::make_unique<service_core_impl>()}
{
}

service_core::~service_core() = default;

service_core::service_core(service_core&& other)
    : inner_resources{std::move(other)}, impl_{std::move(other.impl_)}
{
}

service_core&
service_core::operator=(service_core&& other)
{
    inner_resources::operator=(std::move(other));
    impl_ = std::move(other.impl_);
    return *this;
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
    // A tag specifies a Docker image, and an app in that image;
    // in addition, there is a single tag for lambda functions.
    // Each different tag has a pool of 4 threads.
    // TODO configurable number of threads in a local compute pool.
    return local_compute_pool_.try_emplace(tag, 4).first->second;
}

} // namespace cradle
