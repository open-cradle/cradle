#ifndef CRADLE_INNER_INTROSPECTION_CONFIG_H
#define CRADLE_INNER_INTROSPECTION_CONFIG_H

#include <string>

namespace cradle {

// Configuration keys related to introspection
struct introspection_config_keys
{
    // (Optional boolean)
    // If true, ~tasklet_admin() deletes even those tasklets that did not
    // finish. Useful in unit tests to get fewer reported memory leaks.
    inline static std::string const FORCE_FINISH{"introspection/force_finish"};
};

} // namespace cradle

#endif
