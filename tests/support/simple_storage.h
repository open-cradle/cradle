#ifndef CRADLE_TESTS_SUPPORT_SIMPLE_STORAGE_H
#define CRADLE_TESTS_SUPPORT_SIMPLE_STORAGE_H

#include <map>
#include <optional>
#include <string>

#include <cppcoro/task.hpp>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/service/secondary_storage_intf.h>

namespace cradle {

// Simple secondary storage allowing blob files, and storing outer blobs as
// they are; similar to a disk cache.
class simple_blob_storage : public secondary_storage_intf
{
 public:
    void
    clear() override;

    cppcoro::task<std::optional<blob>>
    read(std::string key) override;

    cppcoro::task<void>
    write(std::string key, blob value) override;

    bool
    allow_blob_files() const override
    {
        return true;
    }

    int
    size() const
    {
        return static_cast<int>(storage_.size());
    }

 private:
    std::map<std::string, blob> storage_;
};

// Simple secondary storage disallowing blob files, and storing outer blobs as
// byte sequences; similar to an HTTP cache.
class simple_string_storage : public secondary_storage_intf
{
 public:
    void
    clear() override;

    cppcoro::task<std::optional<blob>>
    read(std::string key) override;

    cppcoro::task<void>
    write(std::string key, blob value) override;

    bool
    allow_blob_files() const override
    {
        return false;
    }

    int
    size() const
    {
        return static_cast<int>(storage_.size());
    }

 private:
    std::map<std::string, std::string> storage_;
};

} // namespace cradle

#endif
