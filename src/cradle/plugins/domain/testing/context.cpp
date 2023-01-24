#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/context.h>

namespace cradle {

testing_request_context::testing_request_context(
    inner_resources& service, tasklet_tracker* tasklet, bool remotely)
    : service{service}, remotely_{remotely}
{
    if (tasklet)
    {
        tasklets_.push_back(tasklet);
    }
}

inner_resources&
testing_request_context::get_resources()
{
    return service;
}

immutable_cache&
testing_request_context::get_cache()
{
    return service.memory_cache();
}

tasklet_tracker*
testing_request_context::get_tasklet()
{
    if (tasklets_.empty())
    {
        return nullptr;
    }
    return tasklets_.back();
}

void
testing_request_context::push_tasklet(tasklet_tracker* tasklet)
{
    tasklets_.push_back(tasklet);
}

void
testing_request_context::pop_tasklet()
{
    tasklets_.pop_back();
}

void
testing_request_context::proxy_name(std::string const& name)
{
    proxy_name_ = name;
}

std::string const&
testing_request_context::proxy_name() const
{
    if (proxy_name_.empty())
    {
        throw std::logic_error(
            "testing_request_context::proxy_name(): name not set");
    }
    return proxy_name_;
}

std::unique_ptr<remote_context_intf>
testing_request_context::local_clone() const
{
    return std::make_unique<testing_request_context>(service, nullptr);
}

std::shared_ptr<blob_file_writer>
testing_request_context::make_blob_file_writer(size_t size)
{
    return service.make_blob_file_writer(size);
}

} // namespace cradle
