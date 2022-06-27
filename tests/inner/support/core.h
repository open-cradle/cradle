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
};

} // namespace cradle

#endif
