#ifndef CRADLE_TESTS_INNER_DLL_ADDER_V1_ADDER_V1_DEFS_H
#define CRADLE_TESTS_INNER_DLL_ADDER_V1_ADDER_V1_DEFS_H

#include <string>

#include <cradle/inner/requests/request_props.h>

namespace cradle {

using adder_v1_normal_props
    = request_props<caching_level_type::none, request_function_t::plain, true>;

using adder_v1_proxy_props = request_props<
    caching_level_type::none,
    request_function_t::proxy_plain,
    true,
    proxy_retrier>;

static inline std::string const adder_v1p_uuid{"test-adder-v1p"};
static inline std::string const adder_v1n_uuid{"test-adder-v1n"};
static inline std::string const adder_v1_title{"test-adder"};

template<caching_level_type Level>
using coro_v1_normal_props
    = request_props<Level, request_function_t::coro, true>;

using coro_v1_proxy_props = request_props<
    caching_level_type::none,
    request_function_t::proxy_coro,
    true,
    proxy_retrier>;

static inline std::string const coro_v1p_uuid{"test-coro-v1p"};
static inline std::string const coro_v1n_uuid{"test-coro-v1n"}; // +Level
static inline std::string const coro_v1_title{"test-coro"};

static inline constexpr int adder_v1_b_throw{-1};
static inline constexpr int adder_v1_b_crash{-2};

} // namespace cradle

#endif
