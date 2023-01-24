#ifndef CRADLE_TYPING_CORE_FMT_FORMAT_H
#define CRADLE_TYPING_CORE_FMT_FORMAT_H

// Formatters for the fmt library

#include <sstream>

#include <fmt/format.h>

#include <cradle/typing/core/dynamic.h>
#include <cradle/typing/core/type_definitions.h>

template<>
struct fmt::formatter<cradle::dynamic>
{
    constexpr auto
    parse(format_parse_context& ctx) -> decltype(ctx.begin())
    {
        // No presentation format (yet?) supported
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
        {
            // This (intentionally) raises a compile-time error so cannot be
            // covered in a unit test.
            throw fmt::format_error("invalid format");
        }
        return it;
    }

    template<typename FormatContext>
    auto
    format(cradle::dynamic const& x, FormatContext& ctx) const
        -> decltype(ctx.out())
    {
        std::ostringstream ss;
        ss << x;
        return fmt::format_to(ctx.out(), "{}", ss.str());
    }
};

#endif
