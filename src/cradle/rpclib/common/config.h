#ifndef CRADLE_RPCLIB_COMMON_CONFIG_H
#define CRADLE_RPCLIB_COMMON_CONFIG_H

#include <string>

namespace cradle {

// Configuration keys related to the rpclib client / server
struct rpclib_config_keys
{
    // (Optional number)
    // Port number on which the rpclib server is listening.
    inline static std::string const PORT_NUMBER{"rpclib/port_number"};

    // (Optional boolean)
    // Indicates whether the rpclib server should run in contained mode
    // (just calling a function, no caching or what).
    inline static std::string const CONTAINED{"rpclib/contained"};

    // (Optional integer)
    // How many root requests can run in parallel, on the rpclib server;
    // defines the size of a thread pool shared between synchronous and
    // asynchronous requests.
    inline static std::string const REQUEST_CONCURRENCY{
        "rpclib/request_concurrency"};

    // (Optional boolean)
    // If true, expects a running server; client shouldn't start it, and it is
    // an error if no server is listening on the specified port.
    // If false, client tries to start a server on the specified port if it
    // doesn't detect an existing one.
    inline static std::string const EXPECT_SERVER{"rpclib/expect_server"};
};

} // namespace cradle

#endif
