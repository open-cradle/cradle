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

    // proxy_name identifies the proxy to use if remotely is true
    std::shared_ptr<sync_context_intf>
    make_sync_context(
        inner_resources& resources,
        bool remotely,
        std::string proxy_name) override;

    // proxy_name identifies the proxy to use if remotely is true
    std::shared_ptr<async_context_intf>
    make_async_context(
        inner_resources& resources,
        bool remotely,
        std::string proxy_name) override;
};

/*
 * Registers and initializes the testing domain.
 */
void
register_and_initialize_testing_domain();

} // namespace cradle

#endif
