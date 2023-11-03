#ifndef CRADLE_INNER_CORE_EXCEPTION_H
#define CRADLE_INNER_CORE_EXCEPTION_H

#include <stdexcept>
#include <string>

#include <boost/exception/all.hpp>
#include <boost/stacktrace.hpp>
#include <fmt/format.h>

namespace cradle {

// The following macros are simple wrappers around Boost.Exception to codify
// how that library should be used within CRADLE.

#define CRADLE_DEFINE_EXCEPTION(id)                                           \
    struct id : virtual boost::exception, virtual std::exception              \
    {                                                                         \
        char const*                                                           \
        what() const noexcept                                                 \
        {                                                                     \
            return boost::diagnostic_information_what(*this);                 \
        }                                                                     \
    };

#define CRADLE_DEFINE_ERROR_INFO(T, id)                                       \
    typedef boost::error_info<struct id##_info_tag, T> id##_info;

CRADLE_DEFINE_ERROR_INFO(boost::stacktrace::stacktrace, stacktrace)

#define CRADLE_THROW(x)                                                       \
    BOOST_THROW_EXCEPTION(                                                    \
        (x) << stacktrace_info(boost::stacktrace::stacktrace()))

using boost::get_error_info;

// get_required_error_info is just like get_error_info except that it requires
// the info to be present and returns a const reference to it. If the info is
// missing, it throws its own exception.
CRADLE_DEFINE_EXCEPTION(missing_error_info)
CRADLE_DEFINE_ERROR_INFO(std::string, error_info_id)
CRADLE_DEFINE_ERROR_INFO(std::string, wrapped_exception_diagnostics)
template<class ErrorInfo, class Exception>
typename ErrorInfo::error_info::value_type const&
get_required_error_info(Exception const& e)
{
    typename ErrorInfo::error_info::value_type const* info
        = get_error_info<ErrorInfo>(e);
    if (!info)
    {
        CRADLE_THROW(
            missing_error_info()
            << error_info_id_info(typeid(ErrorInfo).name())
            << wrapped_exception_diagnostics_info(
                   boost::diagnostic_information(e)));
    }
    return *info;
}

class not_implemented_error : public std::logic_error
{
 public:
    not_implemented_error() : std::logic_error{"Not implemented"}
    {
    }

    not_implemented_error(std::string const& what)
        : std::logic_error{fmt::format("Not implemented: {}", what)}
    {
    }

    not_implemented_error(char const* what)
        : std::logic_error{fmt::format("Not implemented: {}", what)}
    {
    }
};

// invalid_enum_value is thrown when an enum's raw (integer) value is invalid.
CRADLE_DEFINE_EXCEPTION(invalid_enum_value)
CRADLE_DEFINE_ERROR_INFO(std::string, enum_id)
CRADLE_DEFINE_ERROR_INFO(int, enum_value)

// invalid_enum_string is thrown when attempting to convert a string value to
// an enum and the string doesn't match any of the enum's cases.
CRADLE_DEFINE_EXCEPTION(invalid_enum_string)
// Note that this also uses the enum_id info declared above.
CRADLE_DEFINE_ERROR_INFO(std::string, enum_string)

} // namespace cradle

#endif
