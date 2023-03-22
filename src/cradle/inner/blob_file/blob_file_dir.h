#ifndef CRADLE_INNER_CORE_BLOB_FILE_DIR_H
#define CRADLE_INNER_CORE_BLOB_FILE_DIR_H

#include <filesystem>
#include <mutex>
#include <string>

#include <spdlog/spdlog.h>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/fs/app_dirs.h>
#include <cradle/inner/fs/types.h>
#include <cradle/inner/service/config.h>

namespace cradle {

// Configuration keys for the blob file cache directory.
struct blob_cache_config_keys
{
    // (Optional string)
    inline static std::string const DIRECTORY{"blob_cache/directory"};
};

// Directory where blob files are created.
class blob_file_directory
{
 public:
    blob_file_directory(service_config const& config);

    file_path
    path() const
    {
        return path_;
    }

    // Returns the path to a newly to-be-created blob file
    file_path
    allocate_file();

 private:
    std::mutex mutex_;
    std::shared_ptr<spdlog::logger> logger_;
    file_path path_;
    int next_file_id_{};

    void
    scan_directory();

    file_path
    next_file_path();
};

} // namespace cradle

#endif
