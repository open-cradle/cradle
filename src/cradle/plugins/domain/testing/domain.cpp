#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/context.h>
#include <cradle/plugins/domain/testing/domain.h>

namespace cradle {

testing_domain::testing_domain(inner_resources& resources)
    : resources_{resources}, cat_{resources.get_seri_registry()}
{
}

std::string
testing_domain::name() const
{
    return "testing";
}

std::shared_ptr<local_sync_context_intf>
testing_domain::make_local_sync_context(service_config const& config) const
{
    return std::make_shared<testing_request_context>(resources_, "");
}

std::shared_ptr<root_local_async_context_intf>
testing_domain::make_local_async_context(service_config const& config) const
{
    // Creating an atst_context object should also work, but its flexibility is
    // not needed here.
    return std::make_shared<root_local_atst_context>(
        std::make_unique<local_tree_context_base>(resources_), config);
}

} // namespace cradle
