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

std::shared_ptr<local_sync_context_intf>
thinknode_domain::make_local_sync_context(service_config const& config) const
{
    return std::make_shared<thinknode_request_context>(resources_, config);
}

std::shared_ptr<root_local_async_context_intf>
thinknode_domain::make_local_async_context(service_config const& config) const
{
    return std::make_shared<root_local_async_thinknode_context>(
        std::make_unique<local_tree_context_base>(resources_), config);
}

} // namespace cradle
