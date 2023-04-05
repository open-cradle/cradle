#include <stdexcept>

#include <cradle/inner/service/resources.h>
#include <cradle/thinknode/context.h>
#include <cradle/typing/service/core.h>

namespace cradle {

thinknode_request_context::thinknode_request_context(
    service_core& service,
    thinknode_session session,
    tasklet_tracker* tasklet,
    bool remotely)
    : service{service}, session{std::move(session)}, remotely_{remotely}
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

void
thinknode_request_context::proxy_name(std::string const& name)
{
    proxy_name_ = name;
}

std::string const&
thinknode_request_context::proxy_name() const
{
    if (proxy_name_.empty())
    {
        throw std::logic_error(
            "thinknode_request_context::proxy_name(): name not set");
    }
    return proxy_name_;
}

std::shared_ptr<remote_context_intf>
thinknode_request_context::local_clone() const
{
    return std::make_shared<thinknode_request_context>(
        service, session, nullptr);
}

} // namespace cradle
