#ifndef CRADLE_TESTS_SUPPORT_COMMON_H
#define CRADLE_TESTS_SUPPORT_COMMON_H

#include <memory>
#include <string>

namespace cradle {

class inner_resources;
class service_config;

// Specifies the domain, if any, to register for the current test.
class domain_option
{
 public:
    virtual ~domain_option() = default;

    // Register domain with existing resources
    virtual void
    register_domain(inner_resources& resources) const
        = 0;

    // Create new resources and register domain with them
    virtual std::unique_ptr<inner_resources>
    create_resources_with_domain(service_config const& config) const = 0;
};

// Specifies that no domain should be registered.
class no_domain_option : public domain_option
{
 public:
    void
    register_domain(inner_resources& resources) const override;

    std::unique_ptr<inner_resources>
    create_resources_with_domain(service_config const& config) const override;
};

// Specifies that the "testing" domain should be registered.
class testing_domain_option : public domain_option
{
 public:
    void
    register_domain(inner_resources& resources) const override;

    std::unique_ptr<inner_resources>
    create_resources_with_domain(service_config const& config) const override;
};

void
init_and_register_proxy(
    inner_resources& resources,
    std::string const& proxy_name,
    domain_option const& domain);

} // namespace cradle

#endif
