#ifndef CRADLE_PLUGINS_DOMAIN_TESTING_REQUESTS_H
#define CRADLE_PLUGINS_DOMAIN_TESTING_REQUESTS_H

#include <cstddef>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/uuid.h>
#include <cradle/plugins/domain/testing/context.h>

namespace cradle {

template<caching_level_type Level>
using testing_request_props
    = request_props<Level, true, true, testing_request_context>;

cppcoro::task<blob>
make_some_blob(testing_request_context ctx, std::size_t size, bool shared);

template<caching_level_type Level>
auto
rq_make_some_blob(std::size_t size, bool shared)
{
    return rq_function_erased(
        testing_request_props<Level>(
            request_uuid{"make_some_blob"}, std::string{"make_some_blob"}),
        make_some_blob,
        size,
        shared);
}

template<caching_level_type Level>
using atst_request_props
    = request_props<Level, true, true, local_atst_context>;

cppcoro::task<int>
cancellable_coro(local_async_context_intf& ctx, int loops, int delay);

template<caching_level_type Level, typename Loops, typename Delay>
auto
rq_cancellable_coro(Loops loops, Delay delay)
{
    using props_type = atst_request_props<Level>;
    // TODO uuid should depend on Level
    request_uuid uuid{"cancellable_coro"};
    std::string title{"cancellable_coro"};
    return rq_function_erased(
        props_type(std::move(uuid), std::move(title)),
        cancellable_coro,
        normalize_arg<int, props_type>(std::move(loops)),
        normalize_arg<int, props_type>(std::move(delay)));
}

} // namespace cradle

#endif
