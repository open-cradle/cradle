#ifndef CRADLE_PLUGINS_DOMAIN_TESTING_DOMAIN_H
#define CRADLE_PLUGINS_DOMAIN_TESTING_DOMAIN_H

#include <cradle/inner/requests/domain.h>
#include <cradle/inner/service/resources.h>
#include <cradle/plugins/domain/testing/testing_seri_catalog.h>

namespace cradle {

class testing_domain : public domain
{
 public:
    testing_domain(inner_resources& resources);

    ~testing_domain() = default;

    std::string
    name() const override;

    std::shared_ptr<sync_context_intf>
    make_local_sync_context(service_config const& config) const override;

    std::shared_ptr<async_context_intf>
    make_local_async_context(service_config const& config) const override;

 private:
    inner_resources& resources_;
    testing_seri_catalog cat_;
};

} // namespace cradle

#endif
