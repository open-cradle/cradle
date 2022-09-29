#ifndef CRADLE_WEBSOCKET_CATALOG_H
#define CRADLE_WEBSOCKET_CATALOG_H

/**
 * A catalog of requests that can be deserialized and resolved via the
 * Websocket interface
 */

#include <string>

#include <cppcoro/task.hpp>

#include <cradle/thinknode/types.hpp>
#include <cradle/typing/core/type_definitions.h>

namespace cradle {

/**
 * Creates the catalog
 */
void
create_requests_catalog();

/**
 * Resolves a serialized request from the catalog to a dynamic
 *
 * Resolving a request yields a value with a request-dependent type, such as
 * blob or string.
 * Our Websocket protocol (messages.hpp) must specify the result type sent
 * back across the interface. Converting the request result to a dynamic is
 * convenient on the C++ side and Python can easily handle them. As long as
 * we are dealing with Thinknode requests only, choosing dynamic as the common
 * return type is not really a limitation; otherwise, a MessagePack-encoded
 * blob looks also possible and more general.
 */
cppcoro::task<dynamic>
resolve_serialized_request(
    thinknode_request_context& ctx, std::string const& json_text);

} // namespace cradle

#endif
