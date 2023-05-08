#include <cradle/inner/service/seri_catalog.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/plugins/domain/testing/seri_catalog.h>
#include <cradle/plugins/serialization/secondary_cache/preferred/cereal/cereal.h>

namespace cradle {

namespace {

template<Request Req>
void
register_testing_resolver(Req const& req)
{
    // TODO it should also be possible to resolve make_some_blob in async ctx
    using Ctx = testing_request_context;
    register_seri_resolver<Ctx, Req>(req);
}

template<Request Req>
void
register_atst_resolver(Req const& req)
{
    using Ctx = local_atst_context;
    register_seri_resolver<Ctx, Req>(req);
}

} // namespace

void
register_testing_seri_resolvers()
{
    register_testing_resolver(
        rq_make_some_blob<caching_level_type::full>(1, false));
    register_atst_resolver(
        rq_cancellable_coro<caching_level_type::memory>(0, 0));
}

} // namespace cradle
