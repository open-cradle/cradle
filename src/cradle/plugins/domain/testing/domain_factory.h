#ifndef CRADLE_PLUGINS_DOMAIN_TESTING_DOMAIN_FACTORY_H
#define CRADLE_PLUGINS_DOMAIN_TESTING_DOMAIN_FACTORY_H

#include <memory>

#include <cradle/inner/requests/domain.h>
#include <cradle/inner/service/resources.h>

// Note:
//   #include <cradle/plugins/domain/testing/domain.h>
// leads to build errors due to msgpack conflicts.

namespace cradle {

/*
 * Creates and initializes the testing domain.
 */
std::unique_ptr<domain>
create_testing_domain(inner_resources& resources);

} // namespace cradle

#endif
