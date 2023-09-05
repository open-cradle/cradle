#ifndef CRADLE_INNER_REMOTE_CONFIG_H
#define CRADLE_INNER_REMOTE_CONFIG_H

#include <string>

namespace cradle {

// Configuration keys related to remote execution
struct remote_config_keys
{
    // (Mandatory string)
    // Domain name
    inline static std::string const DOMAIN_NAME{"remote/domain"};

    // (Optional number)
    // Tasklet id for the client invoking the remote operation
    inline static std::string const TASKLET_ID{"remote/tasklet_id"};
};

} // namespace cradle

#endif
