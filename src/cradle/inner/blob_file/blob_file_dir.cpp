#include <algorithm>
#include <regex>

#include <fmt/format.h>

#include <cradle/inner/blob_file/blob_file_dir.h>

namespace cradle {

blob_file_directory::blob_file_directory(service_config const& config)
    : logger_{spdlog::get("cradle")}
{
    auto rel_path
        = config.get_optional_string(blob_cache_config_keys::DIRECTORY);
    if (rel_path)
    {
        path_ = std::filesystem::absolute(file_path(*rel_path));
    }
    else
    {
        path_ = get_shared_cache_dir(none, "cradle");
    }
    logger_->info("Using blob directory {}", path_.string());

    std::scoped_lock lock{mutex_};
    std::filesystem::create_directories(path_);
    scan_directory();
}

file_path
blob_file_directory::allocate_file()
{
    std::scoped_lock lock{mutex_};
    auto result{next_file_path()};
    ++next_file_id_;
    return result;
}

// Finds the highest file_id for which a "blob_{file_id}" file exists,
// and sets next_file_id_ to that value, plus 1 (or to 0 if there isn't
// any blob file yet).
void
blob_file_directory::scan_directory()
{
    next_file_id_ = 0;
    for (auto const& entry : std::filesystem::directory_iterator(path_))
    {
        std::regex const blob_re{"blob_(\\d+)"};
        std::smatch blob_match;
        std::string filename{entry.path().filename().string()};
        if (std::regex_match(filename, blob_match, blob_re))
        {
            auto file_id{std::stoi(blob_match[1].str())};
            next_file_id_ = std::max(next_file_id_, file_id + 1);
        }
    }
}

file_path
blob_file_directory::next_file_path()
{
    auto filename = fmt::format("blob_{}", next_file_id_);
    return path_ / filename;
}

} // namespace cradle
