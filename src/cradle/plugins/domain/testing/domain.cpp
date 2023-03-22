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

} // namespace cradle
