#include <stdexcept>

#include <cradle/inner/core/exception.h>
#include <cradle/thinknode/context.h>
#include <cradle/thinknode/domain.h>
#include <cradle/thinknode/seri_catalog.h>
#include <cradle/typing/service/core.h>

namespace cradle {

void
thinknode_domain::initialize()
{
    register_thinknode_seri_resolvers();
}

std::string
thinknode_domain::name() const
{
    return "thinknode";
}

std::shared_ptr<sync_context_intf>
thinknode_domain::make_local_sync_context(
    inner_resources& resources, service_config const& config) const
{
    assert(dynamic_cast<service_core*>(&resources) != nullptr);
    auto& tn_resources = static_cast<service_core&>(resources);
    return std::make_shared<thinknode_request_context>(tn_resources, config);
}

std::shared_ptr<async_context_intf>
thinknode_domain::make_local_async_context(
    inner_resources& resources, service_config const& config) const
{
    throw not_implemented_error(
        "thinknode_domain::make_local_async_context()");
}

void
register_and_initialize_thinknode_domain()
{
    auto the_domain{std::make_shared<thinknode_domain>()};
    register_domain(the_domain);
    the_domain->initialize();
}

} // namespace cradle
