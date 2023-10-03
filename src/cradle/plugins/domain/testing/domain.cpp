#include <cradle/plugins/domain/testing/context.h>
#include <cradle/plugins/domain/testing/domain.h>

namespace cradle {

testing_domain::testing_domain(inner_resources& resources)
    : resources_{resources}
{
    cat_.register_all();
}

std::string
testing_domain::name() const
{
    return "testing";
}

std::shared_ptr<sync_context_intf>
testing_domain::make_local_sync_context(service_config const& config) const
{
    return std::make_shared<testing_request_context>(
        resources_, nullptr, false, "");
}

std::shared_ptr<async_context_intf>
testing_domain::make_local_async_context(service_config const& config) const
{
    auto tree_ctx{std::make_shared<local_atst_tree_context>(resources_)};
    return std::make_shared<local_atst_context>(tree_ctx, config);
}

} // namespace cradle
