#ifndef CRADLE_TESTS_INNER_SUPPORT_CORE_H
#define CRADLE_TESTS_INNER_SUPPORT_CORE_H

namespace cradle {

struct inner_service_core;

} // namespace cradle

void
init_test_inner_service(cradle::inner_service_core& core);

struct uncached_request_resolution_context
{
};

#endif
