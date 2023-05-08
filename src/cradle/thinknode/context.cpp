#include <stdexcept>

#include <cradle/inner/service/resources.h>
#include <cradle/thinknode/context.h>
#include <cradle/typing/service/core.h>

namespace cradle {

thinknode_request_context::thinknode_request_context(
    service_core& service,
    thinknode_session session,
    tasklet_tracker* tasklet,
    bool remotely,
    std::string proxy_name)
    : service{service},
      session{std::move(session)},
      remotely_{remotely},
      proxy_name_{std::move(proxy_name)}
{
    if (tasklet)
    {
        tasklets_.push_back(tasklet);
    }
}

thinknode_request_context::~thinknode_request_context()
{
}

inner_resources&
thinknode_request_context::get_resources()
{
    return service;
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

std::shared_ptr<local_context_intf>
thinknode_request_context::local_clone() const
{
    return std::make_shared<thinknode_request_context>(
        service, session, nullptr, false, "");
}

} // namespace cradle
