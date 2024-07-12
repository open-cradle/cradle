#ifndef CRADLE_INNER_RESOLVE_CREQ_CONTROLLER_H
#define CRADLE_INNER_RESOLVE_CREQ_CONTROLLER_H

/*
 * A creq_controller object controls the resolution of a contained request, in
 * a separate subprocess. The subprocess is an rpclib server instance.
 *
 * A contained request is characterized by having an associated
 * containment_data object, describing how the resolution should happen in the
 * subprocess.
 *
 * The implementation of the request should be in a DLL; the server subprocess
 * is instructed to load that DLL.
 *
 * The resolution is asynchronous, meaning progress is being polled. All async
 * calls have a timeout, so a crashing subprocess causes a "timeout" exception,
 * but does not crash the client (e.g., the main rpclib server).
 *
 * Resolving a contained request first resolves any subrequests to values. The
 * main request's function is then invoked on those values; this is what
 * happens in the subprocess. The original resolution is local, implying a
 * local context. Going to the subprocess requires a remote context; this is
 * accomplished by creating a creq_context object, and setting it as delegate
 * of the original context.
 *
 * As always, if the result of the root request being resolved is already
 * present in a cache, then that cached result will be returned; no subrequest
 * is resolved, and the main function is not executed.
 */

#include <memory>
#include <string>

#include <cppcoro/task.hpp>
#include <spdlog/spdlog.h>

#include <cradle/inner/resolve/seri_result.h>

namespace cradle {

class local_context_intf;
class creq_context;

class creq_controller
{
 public:
    creq_controller(std::string dll_dir, std::string dll_name);

    ~creq_controller();

    cppcoro::task<serialized_result>
    resolve(local_context_intf& ctx, std::string seri_req);

 private:
    std::string dll_dir_;
    std::string dll_name_;
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<creq_context> ctx_;
};

} // namespace cradle

#endif
