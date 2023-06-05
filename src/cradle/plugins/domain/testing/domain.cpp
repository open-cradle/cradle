#include <cradle/inner/core/exception.h>
#include <cradle/plugins/domain/testing/context.h>
#include <cradle/plugins/domain/testing/domain.h>
#include <cradle/plugins/domain/testing/seri_catalog.h>

namespace cradle {

void
testing_domain::initialize()
{
    register_testing_seri_resolvers();
}

std::string
testing_domain::name() const
{
    return "testing";
}

std::shared_ptr<sync_context_intf>
testing_domain::make_sync_context(
    inner_resources& resources, bool remotely, std::string proxy_name)
{
    return std::make_shared<testing_request_context>(
        resources, nullptr, remotely, std::move(proxy_name));
}

std::shared_ptr<async_context_intf>
testing_domain::make_async_context(
    inner_resources& resources, bool remotely, std::string proxy_name)
{
    std::shared_ptr<async_context_intf> ctx;
    if (!remotely)
    {
        auto tree_ctx{std::make_shared<local_atst_tree_context>(resources)};
        ctx = std::make_shared<local_atst_context>(tree_ctx, nullptr, true);
    }
    else
    {
        auto tree_ctx{std::make_shared<proxy_atst_tree_context>(
            resources, std::move(proxy_name))};
        ctx = std::make_shared<root_proxy_atst_context>(tree_ctx, true);
    }
    return ctx;
}

void
register_and_initialize_testing_domain()
{
    auto the_domain{std::make_shared<testing_domain>()};
    register_domain(the_domain);
    the_domain->initialize();
}

} // namespace cradle
