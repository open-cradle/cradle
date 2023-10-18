#ifndef CRADLE_THINKNODE_SERI_V1_H
#define CRADLE_THINKNODE_SERI_V1_H

#include <cradle/inner/resolve/seri_catalog.h>

namespace cradle {

// The Thinknode seri resolvers are available as long as an instance of this
// object exists.
class thinknode_seri_catalog_v1 : public selfreg_seri_catalog
{
 public:
    thinknode_seri_catalog_v1(seri_registry& registry);
};

} // namespace cradle

#endif
