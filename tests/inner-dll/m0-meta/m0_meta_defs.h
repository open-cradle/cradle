#ifndef CRADLE_TESTS_INNER_DLL_M0_META_M0_META_DEFS_H
#define CRADLE_TESTS_INNER_DLL_M0_META_M0_META_DEFS_H

#include <string>
#include <vector>

#include <cradle/inner/requests/function.h>
#include <cradle/inner/requests/generic.h>
#include <cradle/inner/requests/request_props.h>

namespace cradle {

// A meta request resolves to an inner request.
// A metavec request resolves to a vector of inner requests.

static inline std::string const m0_inner_uuid{"test-m0-inner"};
static inline std::string const m0_inner_title{"m0-inner"};
static inline std::string const m0_meta_p_uuid{"test-m0-meta-p"}; // +Level
static inline std::string const m0_meta_n_uuid{"test-m0-meta-n"}; // +Level
static inline std::string const m0_meta_title{"m0-meta"};
static inline std::string const m0_metavec_p_uuid{"test-m0-metavec-p"}; // +Lev
// static inline std::string const m0_metavec_n_uuid{"test-m0-metavec-n"};
static inline std::string const m0_metavec_title{"m0-metavec"};

using m0_inner_value_type = int;
template<caching_level_type Level>
using m0_normal_props_type
    = request_props<Level, request_function_t::coro, true>;
using m0_proxy_props_type = request_props<
    caching_level_type::none,
    request_function_t::proxy_coro,
    true,
    proxy_retrier>;
template<caching_level_type Level>
using m0_object_props_type
    = make_request_object_props_type<m0_normal_props_type<Level>>;

// Inner requests uncached for now
using m0_inner_request_type = function_request<
    m0_inner_value_type,
    m0_object_props_type<caching_level_type::none>>;
// Meta requests can be cached
template<caching_level_type Level>
using m0_meta_request_type
    = function_request<m0_inner_request_type, m0_object_props_type<Level>>;
// Metavec requests can be cached
template<caching_level_type Level>
using m0_metavec_request_type = function_request<
    std::vector<m0_inner_request_type>,
    m0_object_props_type<Level>>;

} // namespace cradle

#endif
