#ifndef CRADLE_WEBSOCKET_INTROSPECTION_H
#define CRADLE_WEBSOCKET_INTROSPECTION_H

#include <cradle/inner/remote/proxy.h>
#include <cradle/websocket/messages.hpp>

namespace cradle {

cradle::introspection_status_response
make_introspection_status_response(remote_proxy& proxy, bool include_finished);

void
introspection_control(cradle::introspection_control_request const& request);

} // namespace cradle

#endif
