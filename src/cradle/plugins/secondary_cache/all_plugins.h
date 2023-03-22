#ifndef CRADLE_PLUGINS_SECONDARY_CACHE_ALL_PLUGINS_H
#define CRADLE_PLUGINS_SECONDARY_CACHE_ALL_PLUGINS_H

#include <string>
#include <vector>

namespace cradle {

std::vector<std::string>
get_secondary_storage_plugin_names();

void
activate_all_secondary_storage_plugins();

} // namespace cradle

#endif
