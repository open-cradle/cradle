#ifndef CRADLE_THINKNODE_UTILITIES_H
#define CRADLE_THINKNODE_UTILITIES_H

#include <cppcoro/task.hpp>
#include <spdlog/spdlog.h>

#include <cradle/service/core.h>
#include <cradle/thinknode/types.hpp>

namespace cradle {

api_type_info
as_api_type(thinknode_type_info const& tn);

string
get_account_name(thinknode_session const& session);

// Get the service associated with the given Thinknode ID.
thinknode_service_id
get_thinknode_service_id(string const& thinknode_id);

inline cppcoro::task<http_response>
async_http_request(thinknode_request_context trc, http_request request)
{
    return async_http_request(trc.service, std::move(request), trc.tasklet);
}

void
log_info(thinknode_request_context& trc, const char* msg);

} // namespace cradle

#endif
