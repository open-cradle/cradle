#ifndef CRADLE_INNER_SERVICE_CONFIG_MAP_TO_JSON_H
#define CRADLE_INNER_SERVICE_CONFIG_MAP_TO_JSON_H

#include <string>

#include <cradle/inner/service/config.h>

namespace cradle {

// Converts a configuration map to a JSON string.
std::string
write_config_map_to_json(service_config_map const& map);

} // namespace cradle

#endif
