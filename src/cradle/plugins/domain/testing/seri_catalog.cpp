#include <cradle/inner/service/seri_registry.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/plugins/domain/testing/seri_catalog.h>

namespace cradle {

namespace {

template<Request Req>
void
register_testing_resolver(Req const& req)
{
    using Ctx = testing_request_context;
    register_seri_resolver<Ctx, Req>(req);
}

} // namespace

void
register_testing_seri_resolvers()
{
    constexpr caching_level_type level = caching_level_type::full;
    register_testing_resolver(rq_make_some_blob<level>(1));
}

} // namespace cradle
