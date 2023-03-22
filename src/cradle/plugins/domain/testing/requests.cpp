#include <spdlog/spdlog.h>

#include <cradle/inner/blob_file/blob_file.h>
#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/core/type_interfaces.h>
#include <cradle/plugins/domain/testing/requests.h>

namespace cradle {

// TODO shared should be used on an RPC server only; how to enforce?
cppcoro::task<blob>
make_some_blob(testing_request_context ctx, std::size_t size, bool shared)
{
    spdlog::get("cradle")->info("make_some_blob({}, {})", size, shared);

    std::shared_ptr<data_owner> owner;
    uint8_t* data{nullptr};
    if (shared)
    {
        auto writer = ctx.make_blob_file_writer(size);
        owner = writer;
        data = writer->data();
    }
    else
    {
        auto buffer{make_shared_buffer(size)};
        owner = buffer;
        data = buffer->data();
    }
    uint8_t v = 0;
    for (std::size_t i = 0; i < size; ++i)
    {
        data[i] = v;
        v = v * 3 + 1;
    }
    owner->on_write_completed();
    co_return blob{std::move(owner), as_bytes(data), size};
}

} // namespace cradle
