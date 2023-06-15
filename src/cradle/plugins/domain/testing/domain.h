#ifndef CRADLE_PLUGINS_DOMAIN_TESTING_DOMAIN_H
#define CRADLE_PLUGINS_DOMAIN_TESTING_DOMAIN_H

#include <cradle/inner/requests/domain.h>

namespace cradle {

class testing_domain : public domain
{
 public:
    ~testing_domain() = default;

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
 * Registers and initializes the testing domain.
 */
void
register_and_initialize_testing_domain();

} // namespace cradle

#endif
