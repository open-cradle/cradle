#include <cradle/inner/requests/containment_data.h>

namespace cradle {

containment_data::containment_data(
    request_uuid plain_uuid, std::string dll_dir, std::string dll_name)
    : plain_uuid_{std::move(plain_uuid)},
      dll_dir_{std::move(dll_dir)},
      dll_name_{std::move(dll_name)}
{
}

std::unique_ptr<containment_data>
containment_data::load(JSONRequestInputArchive& archive)
{
    // Do not try to read an NVP that is not there, and handle the resulting
    // cereal::Exception, as the exception gives a significant overhead.
    std::string plain_uuid_str;
    archive(cereal::make_nvp("plain_uuid", plain_uuid_str));
    if (plain_uuid_str.empty())
    {
        // No containment data
        return {};
    }
    std::string dll_dir;
    std::string dll_name;
    archive(cereal::make_nvp("dll_dir", dll_dir));
    archive(cereal::make_nvp("dll_name", dll_name));
    return std::make_unique<containment_data>(
        request_uuid{std::move(plain_uuid_str)},
        std::move(dll_dir),
        std::move(dll_name));
}

void
containment_data::save(JSONRequestOutputArchive& archive)
{
    // plain_uuid won't be empty
    archive(cereal::make_nvp("plain_uuid", plain_uuid_.str()));
    archive(cereal::make_nvp("dll_dir", dll_dir_));
    archive(cereal::make_nvp("dll_name", dll_name_));
}

void
containment_data::save_nothing(JSONRequestOutputArchive& archive)
{
    // The "no containment data" placeholder is encoded as an empty plain_uuid
    // string, and no dll_dir or dll_name.
    archive(cereal::make_nvp("plain_uuid", std::string{}));
}

} // namespace cradle
