#ifndef CRADLE_RPCLIB_COMMON_COMMON_H
#define CRADLE_RPCLIB_COMMON_COMMON_H

namespace cradle {

using rpclib_port_t = uint16_t;

static const inline rpclib_port_t RPCLIB_PORT_PRODUCTION = 8098;
static const inline rpclib_port_t RPCLIB_PORT_TESTING = 8096;

// Configuration keys for rpclib
struct rpclib_config_keys
{
    // (Optional integer)
    // How many concurrent threads to use for the rpclib client
    inline static std::string const CLIENT_CONCURRENCY{
        "rpclib/client_concurrency"};

    // (Optional integer)
    // How many concurrent threads to use for the rpclib server
    inline static std::string const SERVER_CONCURRENCY{
        "rpclib/server_concurrency"};

    // (Optional integer)
    // How many asynchronous root requests can run in parallel,
    // on the rpclib server
    inline static std::string const ASYNC_CONCURRENCY{
        "rpclib/async_concurrency"};
};

// Response to "resolve" request
// Using a tuple because a struct requires several non-intrusive msgpack
// adapters.
// The first element is the response id; 0 if unused.
// The second element is the response data itself.
using rpclib_response = std::tuple<int, blob>;

} // namespace cradle

#endif
