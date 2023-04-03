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

std::shared_ptr<context_intf>
testing_domain::make_local_context(inner_resources& service)
{
    // TODO need initial tasklet?
    return std::make_shared<testing_request_context>(service, nullptr);
}

void
atst_domain::initialize()
{
    register_atst_seri_resolvers();
}

std::string
atst_domain::name() const
{
    return "atst";
}

std::shared_ptr<context_intf>
atst_domain::make_local_context(inner_resources& service)
{
    auto tree_ctx{std::make_shared<atst_tree_context>(service)};
    bool const is_req{true};
    return std::make_shared<local_atst_context>(tree_ctx, is_req);
}

} // namespace cradle
