#include <atomic>

#include <spdlog/spdlog.h>

#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/plugins/domain/testing/seri_catalog.h>
#include <cradle/plugins/serialization/secondary_cache/preferred/cereal/cereal.h>

namespace cradle {

void
register_testing_seri_resolvers()
{
    static std::atomic<bool> already_done;
    if (already_done.exchange(true, std::memory_order_relaxed))
    {
        auto logger{spdlog::get("cradle")};
        logger->warn(
            "Ignoring spurious register_testing_seri_resolvers() call");
        return;
    }
    static seri_catalog cat;
    cat.register_resolver(
        rq_make_some_blob<caching_level_type::none>(1, false));
    cat.register_resolver(
        rq_make_some_blob<caching_level_type::memory>(1, false));
    cat.register_resolver(
        rq_make_some_blob<caching_level_type::full>(1, false));
    cat.register_resolver(
        rq_cancellable_coro<caching_level_type::memory>(0, 0));
}

} // namespace cradle
