#ifndef CRADLE_THINKNODE_DOMAIN_H
#define CRADLE_THINKNODE_DOMAIN_H

#include <cradle/inner/requests/domain.h>

namespace cradle {

class thinknode_domain : public domain
{
 public:
    ~thinknode_domain() = default;

    void
    initialize() override;

    std::string
    name() const override;

    std::shared_ptr<sync_context_intf>
    make_local_sync_context(
        inner_resources& resources,
        service_config const& config) const override;

    std::shared_ptr<async_context_intf>
    make_local_async_context(
        inner_resources& resources,
        service_config const& config) const override;
};

/*
 * Registers and initializes the thinknode domain.
 */
void
register_and_initialize_thinknode_domain();

} // namespace cradle

#endif
