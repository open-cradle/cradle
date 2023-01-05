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

    std::shared_ptr<context_intf>
    make_local_context(inner_resources& service) override;
};

} // namespace cradle

#endif
