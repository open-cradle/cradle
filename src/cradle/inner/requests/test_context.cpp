#include <cradle/inner/requests/test_context.h>
#include <cradle/plugins/domain/testing/config.h>

// TODO dependency on plugin

namespace cradle {

test_params_context_mixin::test_params_context_mixin(
    service_config const& config)
{
    load_from_config(config);
}

void
test_params_context_mixin::copy_test_params(
    test_params_context_mixin& other) const
{
    if (fail_submit_async_)
    {
        other.fail_submit_async();
    }
    if (submit_async_delay_ > 0)
    {
        other.set_submit_async_delay(submit_async_delay_);
    }
    if (resolve_async_delay_ > 0)
    {
        other.set_resolve_async_delay(resolve_async_delay_);
    }
    if (set_result_delay_ > 0)
    {
        other.set_set_result_delay(set_result_delay_);
    }
}

void
test_params_context_mixin::update_config_map_with_test_params(
    service_config_map& config_map) const
{
    if (fail_submit_async_)
    {
        config_map[testing_config_keys::FAIL_SUBMIT_ASYNC] = true;
    }
    if (submit_async_delay_ > 0)
    {
        config_map[testing_config_keys::SUBMIT_ASYNC_DELAY]
            = static_cast<std::size_t>(submit_async_delay_);
    }
    if (resolve_async_delay_ > 0)
    {
        config_map[testing_config_keys::RESOLVE_ASYNC_DELAY]
            = static_cast<std::size_t>(resolve_async_delay_);
    }
    if (set_result_delay_ > 0)
    {
        config_map[testing_config_keys::SET_RESULT_DELAY]
            = static_cast<std::size_t>(set_result_delay_);
    }
}

void
test_params_context_mixin::load_from_config(service_config const& config)
{
    fail_submit_async_ = config.get_bool_or_default(
        testing_config_keys::FAIL_SUBMIT_ASYNC, false);
    submit_async_delay_ = static_cast<int>(config.get_number_or_default(
        testing_config_keys::SUBMIT_ASYNC_DELAY, 0));
    resolve_async_delay_ = static_cast<int>(config.get_number_or_default(
        testing_config_keys::RESOLVE_ASYNC_DELAY, 0));
    set_result_delay_ = static_cast<int>(config.get_number_or_default(
        testing_config_keys::SET_RESULT_DELAY, 0));
}

} // namespace cradle
