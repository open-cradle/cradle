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
#include <cradle/rpclib/common/port.h>

namespace cradle {

// Protocol defining the rpclib messages.
// Must be identical between client and server (currently always running on
// the same machine).
// Must be increased when the protocol changes.
static const inline std::string RPCLIB_PROTOCOL{"2"};

// Response to "resolve" request
// Using a tuple because a struct requires several non-intrusive msgpack
// adapters.
using rpclib_response
    = std::tuple<long, remote_cache_record_id::value_type, blob>;
// 1. the response id; 0 if unused.
// 2. if set: identifies a memory cache record on the remote that was locked
//    while resolving the request.
// 3. the response data itself.

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
