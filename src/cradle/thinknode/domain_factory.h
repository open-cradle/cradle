#ifndef CRADLE_THINKNODE_DOMAIN_FACTORY_H
#define CRADLE_THINKNODE_DOMAIN_FACTORY_H

#include <memory>

#include <cradle/inner/requests/domain.h>
#include <cradle/thinknode/service/core.h>

namespace cradle {

/*
 * Creates and initializes the Thinknode domain.
 *
 * Does not make Thinknode-specific seri resolvers available; that must be done
 * by creating a thinknode_seri_catalog object (TODO - revise?).
 */
std::unique_ptr<domain>
create_thinknode_domain(service_core& resources);

} // namespace cradle

#endif
