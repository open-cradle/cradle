#ifndef CRADLE_TESTS_SUPPORT_OUTER_SERVICE_H
#define CRADLE_TESTS_SUPPORT_OUTER_SERVICE_H

#include <cradle/inner/service/config.h>

namespace cradle {

service_config
make_outer_tests_config();

// Initialize a service for unit testing purposes.
void
init_test_service(service_config& core);

} // namespace cradle

#endif
