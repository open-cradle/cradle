#include <utility>

#include <cradle/inner/resolve/seri_catalog.h>
#include <cradle/plugins/domain/testing/demo_class.h>
#include <cradle/plugins/domain/testing/demo_class_requests.h>
#include <cradle/plugins/domain/testing/requests.h>
#include <cradle/plugins/domain/testing/testing_seri_catalog.h>

namespace cradle {

testing_seri_catalog::testing_seri_catalog(
    std::shared_ptr<seri_registry> registry)
    : selfreg_seri_catalog{std::move(registry)}
{
    register_resolver(rq_make_some_blob<caching_level_type::none>(1, false));
    register_resolver(rq_make_some_blob<caching_level_type::memory>(1, false));
    register_resolver(rq_make_some_blob<caching_level_type::full>(1, false));
    register_resolver(
        rq_make_some_blob<caching_level_type::full_vb>(1, false));
    register_resolver(rq_cancellable_coro<caching_level_type::memory>(0, 0));
    register_resolver(rq_non_cancellable_func<caching_level_type::none>(0, 0));

    register_resolver(rq_make_demo_class<caching_level_type::none>(0, 0));
    register_resolver(rq_make_demo_class<caching_level_type::memory>(0, 0));
    register_resolver(rq_make_demo_class<caching_level_type::full>(0, 0));
    register_resolver(
        rq_copy_demo_class<caching_level_type::none>(demo_class{0, 0}));
    register_resolver(
        rq_copy_demo_class<caching_level_type::memory>(demo_class{0, 0}));
    register_resolver(
        rq_copy_demo_class<caching_level_type::full>(demo_class{0, 0}));
}

} // namespace cradle
