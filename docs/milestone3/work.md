# Work for milestone 3

## Introduction
This document describes the (initial?) work we have to do in milestone 3.

The (first) major functionality we want to introduce is an RPC server, that
resolves requests on behalf of a number of clients (C++ applications, WebSocket server).
The server's caches are logically shared between clients: if client X needs a (sub-)result
that was already calculated and cached for client Y, then it will immediately get
the cached result; it need not be recalculated. This already should bring a significant
speedup; by also physically sharing the memory cache between clients, the speedup
will even be more significant.

Before we accomplish this, there is technical debt to be resolved.


## Technical debt
### Thinknode requests should use the request-based framework
The major functionality
implemented in the previous milestone was the transition to a request-based framework.
However, this was not completed, the old framework is still there, and used for resolving
the majority of Thinknode requests. The RPC server functionality will work only in
combination with the new framework, as it really needs the new-style requests. We will
have to refactor more code to use the new framework.

- Decide whether we will refactor all Thinknode functionality, or keep the old framework
  as a legacy solution
- Refactor all or the most important Thinknode requests; on behalf of C++ and/or PUMA
  clients (`external` versus `websocket` directories)
- Add more requests to the external C++ API?
- Move refactored code out of `websocket/` to `thinknode/`
- Update tests, possibly create new ones to improve test coverage
- Remove legacy framework if it becomes unused, otherwise clearly mark it as legacy
- Add converted request types to the catalog
- Put all Thinknode requests functionality in a separate plugin (library)


### Remove obsolete / experimental requests code
In the implementation of the request-based framework, various solutions were evaluated.
The ones we more or less rejected are still there, and tend to be a maintenance burden.

Candidates for removal (also related to unit tests)
- Code in `iss_req_class.h` (putting Thinknode requests in special classes)
- `rq_function_up` and `rq_value_up` (a `unique_ptr` to a request)
- `rq_function_sp` and `rq_value_sp` (a `shared_ptr` to a request)

### Issue #230 (fix configuration parsing)
CRADLE configuration was recently revised, but existing configuration files are not parsed
correctly; this should be fixed. Also update the configuration functionality in the
external API.

### Requests serialization
We are able to serialize requests using the `cereal` library. However, the resulting
JSON shows (too) many implementation details, and looks impractically complex.
We might need to improve (maybe even move to MessagePack? One reason being that
cereal seems to have a bug converting maps to JSON).


## RPC server
See [other document](rpc_server.md) for details.

### Basic functionality:
- Some requests can be serialized and routed to the RPC server
- Not all requests; e.g. there is no reason to send simple "value" requests to a server
- Still retain the option to do everything locally; e.g. have a "CRADLE light",
  also the RPC server itself must do this.
- Decide what (meta-)information should be put where: in a request, in the resolution context,
  in request properties; as template argument or inside a struct/class.
  Try to cut down the number of template instantiations.
- As a starter, only synchronous (=blocking) requests

### Shared memory optimization
Sending large blobs across normal RPC channels will be slow, which might in practice
even mean that reusing a cached result from the RPC server would be slower than
recalculating it in-process. A major performance improvement will be to store
these blobs in shared memory, that can be directly accessed by various clients.

See [other document](shared_memory.md) for technical details.

- Create shared blob class
- RPC requests and responses can contain references to shared blobs
- Adapt memory cache so that it can store shared blobs
- More...


## Miscellaneous
More technical debt, and improvement opportunities.

### Flexible design
- Continue driving from the monolithic design
- E.g. option to assemble a "CRADLE light" solution not linking to any RPC code
  (or maybe dummy code that won't get called)
- Continue splitting the `cradle_outer` cmake target

### Possible disk cache optimizations
- No base64 encoding for blobs;
  cf. [SQLite document](https://sqlite.org/fasterthanfs.html)
- Change threshold for externalizing blobs to 100KB;
  cf. [SQLite document](https://sqlite.org/intern-v-extern-blob.html)

### Upgrade used libraries
- E.g. we are using [msgpack](https://github.com/msgpack/msgpack-c) 3.3.0, but Conan has 4.1.2
  (from the active `cpp_master` branch). cmake is complaining about our version being an one one.
  However, when upgrading, one unit test fails.
- rpclib is using a msgpack version based on 2.1.5; can we merge the two?
- Get rid of remaining `picosha2` usages, preferring the faster openssl

### Fewer serialization protocols
We currently have our own protocol, three JSON libraries and two MessagePack ones:
- Native encoding
- rapidjson in cereal
- nlohmann for converting _to_ JSON
- simdjson for converting _from_ JSON
- msgpack 2.1.5 (-derived) in rpclib
- msgpack 3.3.0 over WebSocket (and more?)

### Fewer conversions
We have quite a number of conversions happening, e.g. from/to dynamic, from/to JSON.
We should try to minimize these; e.g. it could be possible to pass on blob read from
the disk cache on the RPC server to a WebSocket message by copying only, without
needing any conversions at all.
