#ifndef CRADLE_INNER_THINKNODE_CONFIG_H
#define CRADLE_INNER_THINKNODE_CONFIG_H

#include <string>

namespace cradle {

// Configuration keys related to Thinknode
struct thinknode_config_keys
{
    // (Mandatory string)
    // Access token
    // TODO secret access tokens passed over RPC - OK?
    inline static std::string const ACCESS_TOKEN{"thinknode/access_token"};

    // (Mandatory string)
    // Thinknode API URL
    inline static std::string const API_URL{"thinknode/api_url"};
};

} // namespace cradle

#endif
