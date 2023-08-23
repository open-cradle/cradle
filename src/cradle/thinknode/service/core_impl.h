#ifndef CRADLE_THINKNODE_SERVICE_CORE_IMPL_H
#define CRADLE_THINKNODE_SERVICE_CORE_IMPL_H

#include <map>
#include <string>
#include <utility>

#include <cppcoro/static_thread_pool.hpp>

#include <cradle/thinknode/types.hpp>

namespace cradle {

class service_core_impl
{
 public:
    cppcoro::static_thread_pool&
    get_local_compute_pool_for_image(
        std::pair<std::string, thinknode_provider_image_info> const& tag);

 private:
    std::map<
        std::pair<std::string, thinknode_provider_image_info>,
        cppcoro::static_thread_pool>
        local_compute_pool_;
};

} // namespace cradle

#endif
