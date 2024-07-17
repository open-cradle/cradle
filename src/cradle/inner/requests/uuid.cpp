#include <stdexcept>

#include <fmt/format.h>
#include <msgpack.hpp>

#include <cradle/inner/encodings/msgpack_packer.h>
#include <cradle/inner/requests/uuid.h>
#include <cradle/version_info.h>

namespace cradle {

namespace {

std::string
make_base_string(std::string const& orig)
{
    auto ext_pos = orig.find_first_of('+');
    if (ext_pos == std::string::npos)
    {
        return orig;
    }
    // Strip away the extension(s)
    return orig.substr(0, ext_pos);
}

} // namespace

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

request_uuid::request_uuid(std::string complete, complete_tag)
    : str_{std::move(complete)}, finalized_{true}
{
}

request_uuid
request_uuid::clone() const
{
    request_uuid res{make_base_string(str_)};
    if (include_level_)
    {
        res.set_level(level_);
    }
    if (flattened_)
    {
        res.set_flattened();
    }
    return res;
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
request_uuid::set_flattened()
{
    check_not_finalized();
    // Catch attempts to clone a cloned request.
    // Flattening a flattened uuid could also be a no-op.
    if (flattened_)
    {
        throw std::logic_error("request_uuid object already flattened");
    }
    flattened_ = true;
    return *this;
}

void
request_uuid::save(msgpack_packer& packer) const
{
    packer.pack(str());
}

template<>
request_uuid
request_uuid::load(msgpack::object const& msgpack_obj)
{
    request_uuid uuid;
    msgpack_obj.convert(uuid.str_);
    return uuid;
}

void
request_uuid::check_not_finalized() const
{
    if (finalized_)
    {
        throw std::logic_error("request_uuid object already finalized");
    }
}

void
request_uuid::do_finalize() const
{
    if (include_level_)
    {
        // level_exts should match the caching_level_type definition.
        static const char* const level_exts[]
            = {"+none", "+mem", "+full", "+mem_vb", "+full_vb"};
        str_ += level_exts[static_cast<int>(level_)];
    }
    if (flattened_)
    {
        str_ += "+flattened";
    }
    finalized_ = true;
}

} // namespace cradle
