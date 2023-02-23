#ifndef CRADLE_INNER_SERVICE_SECONDARY_CACHED_BLOB_H
#define CRADLE_INNER_SERVICE_SECONDARY_CACHED_BLOB_H

#include <functional>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/id.h>
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/service/resources.h>

namespace cradle {

// Resolves a blob request, using the secondary cache provided by the given
// resources.
cppcoro::task<blob>
secondary_cached_blob(
    inner_resources& resources,
    captured_id key,
    std::function<cppcoro::task<blob>()> create_task);

} // namespace cradle

#endif
