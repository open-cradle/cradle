#ifndef CRADLE_TESTS_INNER_SUPPORT_CORE_H
#define CRADLE_TESTS_INNER_SUPPORT_CORE_H

#include <cradle/inner/service/core.h>

void
init_test_inner_service(cradle::inner_service_core& core);

struct uncached_request_resolution_context
{
};

struct memory_cached_request_resolution_context
{
    cradle::inner_service_core service;

    memory_cached_request_resolution_context();
};

#endif
