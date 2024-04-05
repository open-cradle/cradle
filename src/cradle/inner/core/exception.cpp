#include <cradle/inner/core/exception.h>
#include <cradle/inner/utilities/errors.h>

namespace cradle {

// Specialized for exceptions having an internal_error_message_info string,
// being a one-line error message.
std::string
short_what(std::exception const& e)
{
    if (auto const* msg
        = boost::get_error_info<internal_error_message_info>(e))
    {
        return *msg;
    }
    return e.what();
}

} // namespace cradle
