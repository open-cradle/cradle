#include <cradle/inner/core/exception.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/plugins/secondary_cache/simple/simple_storage.h>

namespace cradle {

void
simple_blob_storage::clear()
{
    throw not_implemented_error();
}

cppcoro::task<std::optional<blob>>
simple_blob_storage::read(std::string key)
{
    auto it = storage_.find(key);
    co_return it != storage_.end() ? std::make_optional(it->second)
                                   : std::nullopt;
}

cppcoro::task<void>
simple_blob_storage::write(std::string key, blob value)
{
    storage_[key] = value;
    co_return;
}

void
simple_string_storage::clear()
{
    throw not_implemented_error();
}

cppcoro::task<std::optional<blob>>
simple_string_storage::read(std::string key)
{
    auto it = storage_.find(key);
    co_return it != storage_.end() ? std::make_optional(make_blob(it->second))
                                   : std::nullopt;
}

cppcoro::task<void>
simple_string_storage::write(std::string key, blob value)
{
    storage_[key] = to_string(value);
    co_return;
}

} // namespace cradle
