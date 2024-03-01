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

    // (Optional boolean)
    // If true, the memory cache record for the request being resolved should
    // be locked on behalf of the caller, until the caller releases the lock.
    inline static std::string const NEED_RECORD_LOCK{
        "remote/need_record_lock"};
};

} // namespace cradle

#endif
