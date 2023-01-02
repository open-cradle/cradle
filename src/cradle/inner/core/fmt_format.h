#ifndef CRADLE_INNER_CORE_FMT_FORMAT_H
#define CRADLE_INNER_CORE_FMT_FORMAT_H

// Formatters for the fmt library

#include <sstream>

#include <fmt/format.h>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>

template<>
struct fmt::formatter<cradle::blob>
{
    constexpr auto
    parse(format_parse_context& ctx) -> decltype(ctx.begin())
    {
        // No presentation format (yet?) supported
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}')
        {
            throw fmt::format_error("invalid format");
        }
        return it;
    }

    template<typename FormatContext>
    auto
    format(cradle::blob const& b, FormatContext& ctx) const
        -> decltype(ctx.out())
    {
        std::ostringstream ss;
        ss << b;
        return fmt::format_to(ctx.out(), "{}", ss.str());
    }
};

#endif
