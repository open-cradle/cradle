#include <cradle/inner/service/internals.h>
#include <cradle/inner/service/types.h>
#include <cradle/thinknode/types.hpp>
#include <cradle/typing/service/core.h>

namespace cradle {

thinknode_request_context::thinknode_request_context(
    service_core& service, thinknode_session session, tasklet_tracker* tasklet)
    : service{service}, session{std::move(session)}
{
    if (tasklet)
    {
        tasklets_.push_back(tasklet);
    }
}

inner_service_core&
thinknode_request_context::get_service()
{
    return service;
}

immutable_cache&
thinknode_request_context::get_cache()
{
    return service.inner_internals().cache;
}

tasklet_tracker*
thinknode_request_context::get_tasklet()
{
    if (tasklets_.empty())
    {
        return nullptr;
    }
    return tasklets_.back();
}

void
thinknode_request_context::push_tasklet(tasklet_tracker* tasklet)
{
    tasklets_.push_back(tasklet);
}

void
thinknode_request_context::pop_tasklet()
{
    tasklets_.pop_back();
}

} // namespace cradle
