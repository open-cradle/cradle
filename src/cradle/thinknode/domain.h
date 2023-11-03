#ifndef CRADLE_THINKNODE_DOMAIN_H
#define CRADLE_THINKNODE_DOMAIN_H

#include <cradle/inner/requests/domain.h>
#include <cradle/thinknode/service/core.h>

namespace cradle {

// Factory of contexts needed for resolving a Thinknode request.
// TODO add Thinknode seri resolvers?
class thinknode_domain : public domain
{
 public:
    thinknode_domain(service_core& resources);

    std::string
    name() const override;

    std::shared_ptr<sync_context_intf>
    make_local_sync_context(service_config const& config) const override;

    std::shared_ptr<async_context_intf>
    make_local_async_context(service_config const& config) const override;

 private:
    service_core& resources_;
};

} // namespace cradle

#endif
