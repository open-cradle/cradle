#include <cradle/inner/core/exception.h>
#include <cradle/thinknode/context.h>
#include <cradle/thinknode/domain.h>

namespace cradle {

thinknode_domain::thinknode_domain(service_core& resources)
    : resources_{resources}
{
}

std::string
thinknode_domain::name() const
{
    return "thinknode";
}

std::shared_ptr<sync_context_intf>
thinknode_domain::make_local_sync_context(service_config const& config) const
{
    return std::make_shared<thinknode_request_context>(resources_, config);
}

std::shared_ptr<async_context_intf>
thinknode_domain::make_local_async_context(service_config const& config) const
{
    throw not_implemented_error(
        "thinknode_domain::make_local_async_context()");
}

} // namespace cradle
