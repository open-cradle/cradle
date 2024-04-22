#ifndef CRADLE_PLUGINS_DOMAIN_TESTING_DEMO_CLASS_REQUESTS_H
#define CRADLE_PLUGINS_DOMAIN_TESTING_DEMO_CLASS_REQUESTS_H

// Requests related to demo_class.
// For remote resolution, resolvers need to be registered in a catalog; see
// testing_seri_catalog.cpp.

#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/request_props.h>
#include <cradle/inner/requests/uuid.h>
#include <cradle/plugins/domain/testing/demo_class.h>

namespace cradle {

// Resolves to a demo_class object
cppcoro::task<demo_class>
make_demo_class(context_intf& ctx, int x, blob y);

template<caching_level_type Level>
auto
rq_make_demo_class(int x, blob y)
{
    constexpr bool introspective{true};
    using props_type
        = request_props<Level, request_function_t::coro, introspective>;
    request_uuid uuid{"make_demo_class"};
    uuid.set_level(Level);
    std::string title{"make_demo_class"};
    return rq_function(
        props_type(std::move(uuid), std::move(title)),
        make_demo_class,
        x,
        std::move(y));
}

// Takes a demo_class object argument, and resolves to one
cppcoro::task<demo_class>
copy_demo_class(context_intf& ctx, demo_class d);

template<caching_level_type Level>
auto
rq_copy_demo_class(demo_class d)
{
    constexpr bool introspective{true};
    using props_type
        = request_props<Level, request_function_t::coro, introspective>;
    request_uuid uuid{"copy_demo_class"};
    uuid.set_level(Level);
    std::string title{"copy_demo_class"};
    return rq_function(
        props_type(std::move(uuid), std::move(title)),
        copy_demo_class,
        std::move(d));
}

} // namespace cradle

#endif
