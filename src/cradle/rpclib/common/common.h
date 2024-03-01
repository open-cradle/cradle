#ifndef CRADLE_RPCLIB_COMMON_COMMON_H
#define CRADLE_RPCLIB_COMMON_COMMON_H

#include <iostream>
#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/introspection/tasklet_info.h>
#include <cradle/inner/remote/types.h>

namespace cradle {

using rpclib_port_t = uint16_t;

static const inline rpclib_port_t RPCLIB_PORT_PRODUCTION = 8098;
static const inline rpclib_port_t RPCLIB_PORT_TESTING = 8096;

// Protocol defining the rpclib messages.
// Must be identical between client and server (currently always running on
// the same machine).
// Must be increased when the protocol changes.
static const inline std::string RPCLIB_PROTOCOL{"2"};

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
// The second element, if set, identifies a memory cache record on the remote
// that was locked while resolving the request.
// The third element is the response data itself.
using rpclib_response
    = std::tuple<long, remote_cache_record_id::value_type, blob>;

using tasklet_event_tuple = std::tuple<uint64_t, std::string, std::string>;
// 1. millis since epoch (note: won't fit in uint32_t)
// 2. tasklet_event_type converted to string
// 3. details

using tasklet_event_tuple_list = std::vector<tasklet_event_tuple>;

using tasklet_info_tuple
    = std::tuple<int, std::string, std::string, int, tasklet_event_tuple_list>;
// 1. own tasklet id
// 2. pool name
// 3. tasklet title
// 4. client tasklet id
// 5. tasklet events

using tasklet_info_tuple_list = std::vector<tasklet_info_tuple>;

tasklet_info_tuple_list
make_info_tuples(tasklet_info_list const& infos);

tasklet_info_list
make_tasklet_infos(tasklet_info_tuple_list const& tuples);

void
dump_tasklet_infos(
    tasklet_info_tuple_list const& tuples, std::ostream& os = std::cout);

} // namespace cradle

#endif
