#ifndef CRADLE_INNER_REQUESTS_NORMALIZATION_UUID_H
#define CRADLE_INNER_REQUESTS_NORMALIZATION_UUID_H

#include <string>

namespace cradle {

// Contains the uuid strings for a normalize_arg request (function.h).
//
// The uuid (only) depends on:
// - The value type that the request resolves to
// - Whether the function is "normal" or a coroutine
// Note: don't put the request_uuid itself in the struct as it depends
// on the static Git version which is also evaluated at C++ initialization
// time.
//
// This file only defines the standard uuid strings. Additional ones should be
// put in a plugin.
// All strings should start with "normalization<" (so that
// function_request_impl can optimize these resolutions).
template<typename Value>
struct normalization_uuid_str
{
};

template<>
struct normalization_uuid_str<int>
{
    static const inline std::string func{"normalization<int,func>"};
    static const inline std::string coro{"normalization<int,coro>"};
};

template<>
struct normalization_uuid_str<std::string>
{
    static const inline std::string func{"normalization<string,func>"};
    static const inline std::string coro{"normalization<string,coro>"};
};

template<>
struct normalization_uuid_str<blob>
{
    static const inline std::string func{"normalization<blob,func>"};
    static const inline std::string coro{"normalization<blob,coro>"};
};

} // namespace cradle

#endif
