#include <stdexcept>

#include <fmt/format.h>

#include <cradle/inner/requests/uuid.h>
#include <cradle/version_info.h>

namespace cradle {

request_uuid::request_uuid(std::string base) : str_{std::move(base)}
{
    // Check the base string for validity:
    // - It must not be empty
    // - '+' prefixes an extension, so is not allowed
    if (str_.empty())
    {
        throw uuid_error{"request_uuid base is empty"};
    }
    if (str_.find_first_of("+") != std::string::npos)
    {
        throw uuid_error{
            fmt::format("Invalid character(s) in request_uuid base {}", str_)};
    }
}

request_uuid&
request_uuid::set_level(caching_level_type level)
{
    check_not_finalized();
    level_ = level;
    include_level_ = true;
    return *this;
}

request_uuid&
request_uuid::use_git_version()
{
    check_not_finalized();
    use_git_version_ = true;
    return *this;
}

void
request_uuid::check_not_finalized() const
{
    if (finalized_)
    {
        throw std::logic_error("request_id object already finalized");
    }
}

void
request_uuid::do_finalize() const
{
    if (include_level_)
    {
        static const char* const level_exts[] = {"+none", "+mem", "+full"};
        str_ += level_exts[static_cast<int>(level_)];
    }
    if (use_git_version_)
    {
        str_ += '+';
        str_ += get_git_version();
    }
    finalized_ = true;
}

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

} // namespace cradle
