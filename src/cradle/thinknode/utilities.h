#ifndef CRADLE_THINKNODE_UTILITIES_H
#define CRADLE_THINKNODE_UTILITIES_H

#include <cppcoro/task.hpp>
#include <spdlog/spdlog.h>

#include <cradle/thinknode/context.h>
#include <cradle/thinknode/service/core.h>
#include <cradle/thinknode/types.hpp>

namespace cradle {

api_type_info
as_api_type(thinknode_type_info const& tn);

string
get_account_name(std::string const& api_url);

string
get_account_name(thinknode_session const& session);

// Get the service associated with the given Thinknode ID.
thinknode_service_id
get_thinknode_service_id(string const& thinknode_id);

cppcoro::task<http_response>
async_http_request(thinknode_request_context ctx, http_request request);

void
log_info(thinknode_request_context& ctx, const char* msg);

} // namespace cradle

#endif
