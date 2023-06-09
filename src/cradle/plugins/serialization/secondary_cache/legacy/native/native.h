#ifndef CRADLE_PLUGINS_SERIALIZATION_SECONDARY_CACHE_LEGACY_NATIVE_NATIVE_H
#define CRADLE_PLUGINS_SERIALIZATION_SECONDARY_CACHE_LEGACY_NATIVE_NATIVE_H

// A plugin serializing disk-cached values (blob or otherwise) using a native
// encoding.
// (Currently) only for old-style requests (fully_cached() calls).
// Any value that is not a blob or a dynamic will first be converted to a
// dynamic.

#include <cppcoro/fmap.hpp>

#include <cradle/inner/service/resources.h>
#include <cradle/inner/service/secondary_cached_blob.h>
#include <cradle/thinknode/caching.h>
#include <cradle/thinknode/secondary_cached.h>
#include <cradle/typing/core/dynamic.h>
#include <cradle/typing/core/type_definitions.h>

namespace cradle {

// The default is to convert a value to a dynamic and serialize that.
template<class Value>
cppcoro::task<Value>
secondary_cached(
    inner_resources& resources,
    captured_id key,
    std::function<cppcoro::task<Value>()> create_task)
{
    return cppcoro::make_task(cppcoro::fmap(
        CRADLE_LAMBDIFY(from_dynamic<Value>),
        secondary_cached<dynamic>(
            resources, key, [create_task = std::move(create_task)]() {
                return cppcoro::make_task(cppcoro::fmap(
                    CRADLE_LAMBDIFY(to_dynamic<Value>), create_task()));
            })));
}

// There is no need to convert a dynamic to a dynamic.
template<>
cppcoro::task<dynamic>
secondary_cached(
    inner_resources& resources,
    captured_id key,
    std::function<cppcoro::task<dynamic>()> create_task);

// A blob will be stored as-is (no serialization needed).
template<>
cppcoro::task<blob>
secondary_cached(
    inner_resources& core,
    captured_id key,
    std::function<cppcoro::task<blob>()> create_task);

} // namespace cradle

#endif
