#include <cradle/thinknode/context.h>

namespace cradle {

thinknode_request_context::thinknode_request_context(
    service_core& service,
    thinknode_session session,
    tasklet_tracker* tasklet,
    bool remotely,
    std::string proxy_name)
    : sync_context_base{service, tasklet, remotely, std::move(proxy_name)},
      service{service},
      session{std::move(session)}
{
}

thinknode_request_context::~thinknode_request_context()
{
}

std::shared_ptr<local_context_intf>
thinknode_request_context::local_clone() const
{
    return std::make_shared<thinknode_request_context>(
        service, session, nullptr, false, "");
}

} // namespace cradle
