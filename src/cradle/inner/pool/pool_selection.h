#ifndef CRADLE_INNER_POOL_POOL_SELECTION_H
#define CRADLE_INNER_POOL_POOL_SELECTION_H

#include <optional>
#include <string>

namespace cradle {

// Resolve the effective pool name for a leaf from the request's runtime pool
// selection and the context's default: the request's value takes precedence
// over the context default. Returns std::nullopt when neither names a pool, in
// which case the leaf takes the existing inline path.
std::optional<std::string>
select_pool_name(
    std::optional<std::string> const& request_pool,
    std::optional<std::string> const& context_default_pool);

} // namespace cradle

#endif
