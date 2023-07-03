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
#include <cradle/plugins/domain/testing/normalization_uuid.h>

namespace cradle {

cppcoro::task<blob>
make_some_blob(context_intf& ctx, std::size_t size, bool use_shared_memory);

template<caching_level_type Level>
auto
rq_make_some_blob(std::size_t size, bool use_shared_memory)
{
    using contexts = ctx_type_list<
        caching_context_intf,
        introspective_context_intf,
        local_async_context_intf>;
    using props_type = request_props<Level, true, true, contexts>;
    request_uuid uuid{"make_some_blob"};
    uuid.set_level(Level);
    std::string title{"make_some_blob"};
    return rq_function_erased(
        props_type(std::move(uuid), std::move(title)),
        make_some_blob,
        size,
        use_shared_memory);
}

cppcoro::task<int>
cancellable_coro(context_intf& ctx, int loops, int delay);

template<caching_level_type Level, typename Loops, typename Delay>
    requires TypedArg<Loops, int> && TypedArg<Delay, int>
auto
rq_cancellable_coro(Loops loops, Delay delay)
{
    using contexts = ctx_type_list<
        local_async_context_intf,
        introspective_context_intf,
        caching_context_intf>;
    using props_type = request_props<Level, true, true, contexts>;
    request_uuid uuid{"cancellable_coro"};
    uuid.set_level(Level);
    std::string title{"cancellable_coro"};
    return rq_function_erased(
        props_type(std::move(uuid), std::move(title)),
        cancellable_coro,
        normalize_arg<int, props_type>(std::move(loops)),
        normalize_arg<int, props_type>(std::move(delay)));
}

} // namespace cradle

#endif
