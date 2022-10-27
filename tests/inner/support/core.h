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
};

struct cached_request_resolution_context : public cached_context_intf
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
