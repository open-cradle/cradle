#include <cradle/plugins/domain/testing/context.h>
#include <cradle/plugins/domain/testing/domain.h>

namespace cradle {

void
testing_domain::initialize()
{
    cat_.register_all();
}

std::string
testing_domain::name() const
{
    return "testing";
}

std::shared_ptr<sync_context_intf>
testing_domain::make_local_sync_context(
    inner_resources& resources, service_config const& config) const
{
    return std::make_shared<testing_request_context>(
        resources, nullptr, false, "");
}

std::shared_ptr<async_context_intf>
testing_domain::make_local_async_context(
    inner_resources& resources, service_config const& config) const
{
    auto tree_ctx{std::make_shared<local_atst_tree_context>(resources)};
    return std::make_shared<local_atst_context>(tree_ctx, config);
}

void
register_and_initialize_testing_domain()
{
    auto the_domain{std::make_shared<testing_domain>()};
    register_domain(the_domain);
    the_domain->initialize();
}

} // namespace cradle
