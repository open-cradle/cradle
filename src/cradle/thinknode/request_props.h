#ifndef CRADLE_THINKNODE_REQUEST_PROPS_H
#define CRADLE_THINKNODE_REQUEST_PROPS_H

#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/request_props.h>

namespace cradle {

// Properties for a Thinknode request:
// - The function is a coroutine
// - Always introspective
// Context should be thinknode_request_context.
// The caching level will typically be "full", but (e.g. for testing purposes)
// supporting other options may be useful.
template<caching_level_type Level>
using thinknode_request_props
    = request_props<Level, request_function_t::coro, true>;

using thinknode_proxy_props = request_props<
    caching_level_type::none,
    request_function_t::proxy_coro,
    true>;

} // namespace cradle

#endif
