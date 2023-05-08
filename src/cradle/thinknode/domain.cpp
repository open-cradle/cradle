#include <cstdlib>
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

void
thinknode_domain::set_session(thinknode_session const& session)
{
    session_ = session;
}

std::string
thinknode_domain::name() const
{
    return "thinknode";
}

std::shared_ptr<sync_context_intf>
thinknode_domain::make_sync_context(
    inner_resources& resources, bool remotely, std::string proxy_name)
{
    ensure_session();
    assert(dynamic_cast<service_core*>(&resources) != nullptr);
    auto& tn_resources = static_cast<service_core&>(resources);
    // TODO need initial tasklet?
    return std::make_shared<thinknode_request_context>(
        tn_resources, session_, nullptr, remotely, std::move(proxy_name));
}

std::shared_ptr<async_context_intf>
thinknode_domain::make_async_context(
    inner_resources& resources, bool remotely, std::string proxy_name)
{
    throw not_implemented_error("thinknode_domain::make_async_context()");
}

void
thinknode_domain::ensure_session()
{
    if (session_.api_url.size() > 0)
    {
        return;
    }
    char const* access_token = std::getenv("CRADLE_THINKNODE_API_TOKEN");
    if (!access_token)
    {
        throw std::logic_error("No session and no CRADLE_THINKNODE_API_TOKEN");
    }
    session_
        = thinknode_session{"https://mgh.thinknode.io/api/v1.0", access_token};
}

} // namespace cradle
