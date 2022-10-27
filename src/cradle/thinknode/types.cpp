#include <cradle/inner/service/resources.h>
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

inner_resources&
thinknode_request_context::get_resources()
{
    return service;
}

immutable_cache&
thinknode_request_context::get_cache()
{
    return service.memory_cache();
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
