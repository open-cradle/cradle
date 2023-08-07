#ifndef CRADLE_PLUGINS_REQUESTS_STORAGE_HTTP_HTTP_REQUESTS_STORAGE_FACTORY_H
#define CRADLE_PLUGINS_REQUESTS_STORAGE_HTTP_HTTP_REQUESTS_STORAGE_FACTORY_H

#include <cradle/inner/service/resources.h>

namespace cradle {

class http_requests_storage_factory : public secondary_storage_factory
{
 public:
    std::unique_ptr<secondary_storage_intf>
    create(inner_resources& resources, service_config const& config) override;
};

} // namespace cradle

#endif
