#ifndef CRADLE_TYPING_SERVICE_INTERNALS_H
#define CRADLE_TYPING_SERVICE_INTERNALS_H

#include <cppcoro/static_thread_pool.hpp>

#include <cradle/thinknode/types.hpp>
#include <cradle/typing/io/mock_http.h>

namespace cradle {

namespace detail {

struct service_core_internals
{
    cppcoro::static_thread_pool http_pool;

    std::map<
        std::pair<string, thinknode_provider_image_info>,
        cppcoro::static_thread_pool>
        local_compute_pool;

    std::unique_ptr<mock_http_session> mock_http;

    // Normally, HTTP requests are dispatched to a thread in the HTTP thread
    // pool. Setting this to true causes them to be evaluated on the calling
    // thread. This should happen only for mock HTTP in benchmark tests, where
    // it tends to give more reliable and consistent timings.
    bool http_is_synchronous{false};
};

} // namespace detail

} // namespace cradle

#endif
