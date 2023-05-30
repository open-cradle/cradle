#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/plugins/domain/testing/seri_catalog.h>
#include <cradle/plugins/serialization/secondary_cache/preferred/cereal/cereal.h>

namespace cradle {

void
register_testing_seri_resolvers()
{
    register_seri_resolver(
        rq_make_some_blob<caching_level_type::full>(1, false));
    register_seri_resolver(
        rq_cancellable_coro<caching_level_type::memory>(0, 0));
}

} // namespace cradle
