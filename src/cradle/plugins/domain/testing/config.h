#ifndef CRADLE_PLUGINS_DOMAIN_TESTING_CONFIG_H
#define CRADLE_PLUGINS_DOMAIN_TESTING_CONFIG_H

#include <string>

namespace cradle {

// Configuration keys related to testing options
struct testing_config_keys
{
    // (Optional bool)
    // Causes submit_async to fail on the remote; default: false
    inline static std::string const FAIL_SUBMIT_ASYNC{
        "testopts/fail_submit_async"};

    // (Optional number)
    // Delay, in ms, for a submit_async RPC call; default: 0
    inline static std::string const SUBMIT_ASYNC_DELAY{
        "testopts/submit_async_delay"};

    // (Optional number)
    // Startup delay, in ms, for resolve_async; default: 0
    inline static std::string const RESOLVE_ASYNC_DELAY{
        "testopts/resolve_async_delay"};

    // (Optional number)
    // Delay, in ms, for a local_atst_context::set_result() call; default: 0
    inline static std::string const SET_RESULT_DELAY{
        "testopts/set_result_delay"};
};

} // namespace cradle

#endif
