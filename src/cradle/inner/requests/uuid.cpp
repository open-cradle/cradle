#include <cradle/inner/requests/uuid.h>
#include <cradle/version_info.h>

namespace cradle {

std::string const&
request_uuid::get_git_version()
{
    if (git_version_.empty())
    {
        git_version_ = ::version_info.commit_object_name;
        if (::version_info.dirty)
        {
            // If there are local modifications, it becomes the user's
            // responsibility to manually remove outdated files like the
            // disk cache.
            git_version_ += "-dirty";
        }
    }
    return git_version_;
}

// Checks a base string for invalid characters:
// - '+' separates base and version parts
void
request_uuid::check_base(std::string const& base)
{
    if (base.find_first_of("+") != std::string::npos)
    {
        std::stringstream os;
        os << "Invalid character(s) in base uuid " << base;
        throw uuid_error(os.str());
    }
}

std::string
request_uuid::combine(std::string const& base, std::string const& version)
{
    return base + '+' + version;
}

} // namespace cradle
