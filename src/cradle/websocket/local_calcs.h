#ifndef CRADLE_WEBSOCKET_LOCAL_CALCS_H
#define CRADLE_WEBSOCKET_LOCAL_CALCS_H

#include <string>
#include <utility>

#include <cppcoro/static_thread_pool.hpp>

#include <cradle/inner/introspection/tasklet.h>
#include <cradle/thinknode/context.h>
#include <cradle/thinknode/service/core.h>
#include <cradle/thinknode/types.hpp>

namespace cradle {

cppcoro::static_thread_pool&
get_local_compute_pool_for_image(
    service_core& service,
    std::pair<std::string, thinknode_provider_image_info> const& tag);

cppcoro::task<dynamic>
perform_local_function_calc(
    thinknode_request_context ctx,
    string context_id,
    string account,
    string app,
    string name,
    std::vector<dynamic> args);

cppcoro::task<dynamic>
coerce_local_calc_result(
    thinknode_request_context ctx,
    string const& context_id,
    thinknode_type_info const& schema,
    dynamic value);

} // namespace cradle

#endif
