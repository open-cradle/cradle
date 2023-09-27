#ifndef CRADLE_TESTS_SUPPORT_THINKNODE_H
#define CRADLE_TESTS_SUPPORT_THINKNODE_H

#include <memory>

#include <cradle/inner/remote/proxy.h>

namespace cradle {

void
ensure_all_domains_registered();

// As long as this object exists, the Thinknode seri resolvers are available
// on the given proxy. proxy is nullptr for local operation.
// Currently limited to thinknode_v1.
class thinknode_catalog_scope
{
 public:
    thinknode_catalog_scope(remote_proxy* proxy);
    ~thinknode_catalog_scope();

 private:
    remote_proxy* proxy_;
};

} // namespace cradle

#endif
