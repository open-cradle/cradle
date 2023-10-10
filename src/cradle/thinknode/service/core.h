#ifndef CRADLE_TYPING_SERVICE_CORE_H
#define CRADLE_TYPING_SERVICE_CORE_H

// Services exposed by the thinknode subsystem.

#include <memory>
#include <string>
#include <utility>

#include <cppcoro/static_thread_pool.hpp>

#include <cradle/inner/service/config.h>
#include <cradle/inner/service/resources.h>
#include <cradle/thinknode/types.hpp>

namespace cradle {

class service_core_impl;

class service_core : public inner_resources
{
 public:
    service_core(service_config const& config);

    ~service_core();

    service_core(service_core const&) = delete;
    service_core&
    operator=(service_core const&)
        = delete;
    service_core(service_core&& other);
    service_core&
    operator=(service_core&& other);

    cppcoro::static_thread_pool&
    get_local_compute_pool_for_image(
        std::pair<std::string, thinknode_provider_image_info> const& tag);

 private:
    std::unique_ptr<service_core_impl> impl_;
};

} // namespace cradle

#endif
