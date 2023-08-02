#ifndef CRADLE_RPCLIB_COMMON_COMMON_H
#define CRADLE_RPCLIB_COMMON_COMMON_H

#include <string>
#include <tuple>

namespace cradle {

using rpclib_port_t = uint16_t;

static const inline rpclib_port_t RPCLIB_PORT_PRODUCTION = 8098;
static const inline rpclib_port_t RPCLIB_PORT_TESTING = 8096;

// Protocol defining the rpclib messages.
// Must be identical between client and server (currently always running on
// the same machine).
// Must be increased when the protocol changes.
static const inline std::string RPCLIB_PROTOCOL{"1"};

// Configuration keys for rpclib
struct rpclib_config_keys
{
    // (Optional integer)
    // How many root requests can run in parallel, on the rpclib server;
    // defines the size of a thread pool shared between synchronous and
    // asynchronous requests.
    inline static std::string const REQUEST_CONCURRENCY{
        "rpclib/request_concurrency"};
};

// Response to "resolve" request
// Using a tuple because a struct requires several non-intrusive msgpack
// adapters.
// The first element is the response id; 0 if unused.
// The second element is the response data itself.
using rpclib_response = std::tuple<int, blob>;

} // namespace cradle

#endif
