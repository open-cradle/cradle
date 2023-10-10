#ifndef CRADLE_PLUGINS_SECONDARY_CACHE_ALL_PLUGINS_H
#define CRADLE_PLUGINS_SECONDARY_CACHE_ALL_PLUGINS_H

#include <memory>
#include <string>
#include <vector>

#include <cradle/inner/service/resources.h>
#include <cradle/inner/service/secondary_storage_intf.h>

namespace cradle {

std::vector<std::string>
get_secondary_storage_plugin_names();

// Returns empty unique_ptr if no secondary storage configured.
std::unique_ptr<secondary_storage_intf>
create_secondary_storage(inner_resources& resources);

} // namespace cradle

#endif
