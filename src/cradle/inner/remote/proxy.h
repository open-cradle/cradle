#ifndef CRADLE_INNER_REMOTE_PROXY_H
#define CRADLE_INNER_REMOTE_PROXY_H

#include <memory>
#include <string>
#include <tuple>

#include <spdlog/spdlog.h>

#include <cradle/inner/requests/types.h>
#include <cradle/inner/resolve/seri_result.h>
#include <cradle/inner/service/config.h>

namespace cradle {

// Thrown if an error occurred on a remote (server), or while communicating
// with a remote.
class remote_error : public std::logic_error
{
 public:
    remote_error(std::string const& what) : std::logic_error(what)
    {
    }

    remote_error(std::string const& what, std::string const& msg)
        : std::logic_error(fmt::format("{}: {}", what, msg))
    {
    }
};

// Minimal descriptor for a child node in an asynchronous context tree on a
// remote.
// This is a tuple because msgpack has built-in support for tuples but not
// for structs.
// The first element is the value identifying the child context.
// The second element is true for a request, false for a plain value.
using remote_context_spec = std::tuple<async_id, bool>;

// Minimal descriptor for the children of a node in an asynchronous context
// tree on a remote.
using remote_context_spec_list = std::vector<remote_context_spec>;

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

/*
 * Proxy for a remote (server) capable of resolving requests, synchronously
 * and/or asynchronously.
 * All remote calls throw on error.
 * TODO Only remote_error should be thrown
 */
class remote_proxy
{
 public:
    virtual ~remote_proxy() = default;

    // Returns the name of this proxy
    virtual std::string
    name() const
        = 0;

    // Returns the logger associated with this proxy
    virtual spdlog::logger&
    get_logger()
        = 0;

    // Resolves a request, synchronously.
    virtual serialized_result
    resolve_sync(service_config config, std::string seri_req)
        = 0;

    // Submits a request for asynchronous resolution.
    // Returns the remote id of the server's remote context associated with
    // the root request in the request tree. Other remote contexts will likely
    // be constructed only when the request is deserialized, and that could
    // take some time.
    virtual async_id
    submit_async(service_config config, std::string seri_req)
        = 0;

    // Returns the specification of the child contexts of the context subtree
    // of which aid is the root.
    // Should be called for the root aid (returned from submit_async) only
    // when its status is SUBS_RUNNING, SELF_RUNNING or FINISHED.
    virtual remote_context_spec_list
    get_sub_contexts(async_id aid)
        = 0;

    // Returns the status of the remote context specified by aid.
    virtual async_status
    get_async_status(async_id aid)
        = 0;

    // Returns an error message
    // Should be called only when status == ERROR
    virtual std::string
    get_async_error_message(async_id aid)
        = 0;

    // Returns the value that request resolution calculated. root_aid should
    // be the return value of a former submit_async() call. The status of the
    // root context should be FINISHED.
    virtual serialized_result
    get_async_response(async_id root_aid)
        = 0;

    // Requests for an asynchronous resolution to be cancelled. aid should
    // specify a context in the tree.
    virtual void
    request_cancellation(async_id aid)
        = 0;

    // Finishes an asynchronous resolution, giving the server a chance to clean
    // up its administration associated with the resolution. Should be called
    // even when the resolution did not finish successfully (e.g. an
    // exception was thrown).
    virtual void
    finish_async(async_id root_aid)
        = 0;

    // Retrieve introspection info
    virtual tasklet_info_tuple_list
    get_tasklet_infos(bool include_finished)
        = 0;

    // Dynamically loads a shared library, making its seri resolvers available
    // on the remote.
    //
    // dir_path is an absolute path to the directory containg the shared
    // library file.
    // dll_name is the library name as specified in CMakeLists.txt.
    // On Linux, dll_name "bla" translates to file name "libbla.so";
    // on Windows, it would be "bla.dll".
    virtual void
    load_shared_library(std::string dir_path, std::string dll_name)
        = 0;

    // Unloads a previously loaded shared library, so that its seri resolvers
    // are no longer available.
    //
    // In the simplest case, dll_name is as for load_shared_library(), and it
    // is an error if the specified DLL is not loaded.
    // As an extension, dll_name may contain a "*", in which case it is
    // interpreted as a regex and all matching DLLs are unloaded; it is not an
    // error if there are no matching DLLs. This is primarily intended to be
    // used in unit tests.
    virtual void
    unload_shared_library(std::string dll_name)
        = 0;

    // Instructs the server to mock all HTTP requests, returning a 200
    // response with response_body for each.
    virtual void
    mock_http(std::string const& response_body)
        = 0;
};

} // namespace cradle

#endif
