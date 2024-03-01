#ifndef CRADLE_WEBSOCKET_INTROSPECTION_H
#define CRADLE_WEBSOCKET_INTROSPECTION_H

#include <cradle/websocket/messages.hpp>

namespace cradle {

class tasklet_admin;
class remote_proxy;

cradle::introspection_status_response
make_introspection_status_response(
    tasklet_admin& admin, remote_proxy& proxy, bool include_finished);

void
introspection_control(
    tasklet_admin& admin,
    cradle::introspection_control_request const& request);

} // namespace cradle

#endif
