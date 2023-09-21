#include <stdexcept>

#include <cradle/inner/requests/function.h>

namespace cradle {

void
check_title_is_valid(std::string const& title)
{
    if (title.empty())
    {
        throw std::invalid_argument{"empty title"};
    }
}

} // namespace cradle
