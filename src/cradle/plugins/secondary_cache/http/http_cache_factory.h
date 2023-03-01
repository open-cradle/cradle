#ifndef CRADLE_PLUGINS_SECONDARY_CACHE_HTTP_HTTP_CACHE_FACTORY_H
#define CRADLE_PLUGINS_SECONDARY_CACHE_HTTP_HTTP_CACHE_FACTORY_H

#include <cradle/inner/service/resources.h>

namespace cradle {

class http_cache_factory : public secondary_cache_factory
{
 public:
    std::unique_ptr<secondary_cache_intf>
    create(inner_resources& resources, service_config const& config) override;
};

} // namespace cradle

#endif
