#ifndef CRADLE_TESTS_INNER_SUPPORT_CORE_H
#define CRADLE_TESTS_INNER_SUPPORT_CORE_H

#include <cradle/inner/core/exception.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/core.h>

namespace cradle {

void
init_test_inner_service(inner_service_core& core);

struct uncached_request_resolution_context : public context_intf
{
    inner_service_core&
    get_service() override
    {
        throw not_implemented_error("No service in this context");
    }

    immutable_cache&
    get_cache() override
    {
        throw not_implemented_error("No cache in this context");
    }

    tasklet_tracker*
    get_tasklet() override
    {
        throw not_implemented_error("No introspection in this context");
    }

    void
    push_tasklet(tasklet_tracker* tasklet) override
    {
        throw not_implemented_error("No introspection in this context");
    }

    void
    pop_tasklet() override
    {
        throw not_implemented_error("No introspection in this context");
    }
};

struct cached_request_resolution_context : public context_intf
{
    inner_service_core service;

    cached_request_resolution_context();

    inner_service_core&
    get_service() override
    {
        return service;
    }

    immutable_cache&
    get_cache() override
    {
        return service.inner_internals().cache;
    }

    tasklet_tracker*
    get_tasklet() override
    {
        return nullptr;
    }

    void
    push_tasklet(tasklet_tracker* tasklet) override
    {
        throw not_implemented_error("tasklet stack not supported");
    }

    void
    pop_tasklet() override
    {
        throw not_implemented_error("tasklet stack not supported");
    }

    void
    reset_memory_cache();
};

template<caching_level_type level>
struct request_resolution_context_struct
{
    using type = cached_request_resolution_context;
};

template<>
struct request_resolution_context_struct<caching_level_type::none>
{
    using type = uncached_request_resolution_context;
};

template<caching_level_type level>
using request_resolution_context =
    typename request_resolution_context_struct<level>::type;

} // namespace cradle

#endif
