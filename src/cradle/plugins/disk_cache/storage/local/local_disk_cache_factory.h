#ifndef CRADLE_PLUGINS_DISK_CACHE_STORAGE_LOCAL_LOCAL_DISK_CACHE_FACTORY_H
#define CRADLE_PLUGINS_DISK_CACHE_STORAGE_LOCAL_LOCAL_DISK_CACHE_FACTORY_H

#include <cradle/inner/service/resources.h>

namespace cradle {

class local_disk_cache_factory : public disk_cache_factory
{
 public:
    std::unique_ptr<disk_cache_intf>
    create(service_config const& config) override;
};

} // namespace cradle

#endif
