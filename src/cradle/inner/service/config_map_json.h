#ifndef CRADLE_INNER_SERVICE_CONFIG_MAP_JSON_H
#define CRADLE_INNER_SERVICE_CONFIG_MAP_JSON_H

#include <string>

#include <cradle/inner/service/config.h>

namespace cradle {

// Reads a configuration map from a JSON string.
service_config_map
read_config_map_from_json(std::string const& json_text);

} // namespace cradle

#endif
