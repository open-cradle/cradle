#include <cassert>
#include <sstream>

#include <cradle/inner/requests/uuid.h>
#include <cradle/version_info.hpp>

namespace cradle {

std::string const&
request_uuid::get_git_version()
{
    if (git_version_.empty())
    {
        git_version_ = ::version_info.commit_object_name;
        // TODO do something with local modifications?
    }
    return git_version_;
}

// Checks a base string for invalid characters:
// - '+' separates base and version parts
// - '/' separates main and sub parts
void
request_uuid::check_base(std::string const& base)
{
    if (base.find_first_of("+/") != std::string::npos)
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

// TODO uuids depend on classes only, could be evaluated compile time?
request_uuid
combined_uuid(request_uuid const& main_uuid, request_uuid const& sub_uuid)
{
    std::string main{main_uuid.str()};
    std::string sub{sub_uuid.str()};
    if (main.empty())
    {
        // TODO Is this valid?
        return sub_uuid;
    }
    if (sub.empty())
    {
        return main_uuid;
    }
    size_t pos = main.find("+");
    assert(pos != std::string::npos);
    std::string main_base{main.substr(0, pos)};
    std::string main_version{main.substr(pos + 1)};
    // Odd but it should do
    return request_uuid(main_base, main_version + '/' + sub);
}

} // namespace cradle
