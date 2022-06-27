#ifndef CRADLE_TESTS_INNER_SUPPORT_CORE_H
#define CRADLE_TESTS_INNER_SUPPORT_CORE_H

#include <cradle/inner/core/exception.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/service/core.h>

namespace cradle {

void
init_test_inner_service(cradle::inner_service_core& core);

struct uncached_request_resolution_context : public context_intf
{

    immutable_cache&
    get_cache() override
    {
        throw not_implemented_error("No cache in this context");
    }
};

struct memory_cached_request_resolution_context : public context_intf
{
    cradle::inner_service_core service;

    memory_cached_request_resolution_context();

    immutable_cache&
    get_cache() override
    {
        return service.inner_internals().cache;
    }
};

} // namespace cradle

#endif
