#ifndef CRADLE_WEBSOCKET_SERVER_H
#define CRADLE_WEBSOCKET_SERVER_H

#include <memory>

#include <cradle/inner/service/config.h>
#include <cradle/typing/core.h>

namespace cradle {

// Configuration keys for the server
struct server_config_keys
{
    // (Optional boolean)
    // Whether or not the server should be open to connections from other
    // machines (defaults to false).
    inline static std::string const OPEN{"open"};

    // (Optional integer)
    // The WebSocket port on which the server will listen.
    inline static std::string const PORT{"port"};
};

CRADLE_DEFINE_EXCEPTION(websocket_server_error)
// This exception provides internal_error_message_info.

// The websocket server uses this type to identify clients.
typedef int websocket_client_id;

class websocket_server_impl;

class websocket_server
{
 public:
    websocket_server(service_config const& config);
    ~websocket_server();

    void
    listen();

    void
    run();

 private:
    std::unique_ptr<websocket_server_impl> impl_;
};

} // namespace cradle

#endif
