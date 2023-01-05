#include <cradle/inner/core/type_definitions.h>
#include <cradle/plugins/domain/testing/requests.h>

namespace cradle {

cppcoro::task<blob>
make_some_blob(testing_request_context, std::size_t size)
{
    auto data{byte_vector(size)};
    byte_vector::value_type v = 0;
    for (auto& it : data)
    {
        it = v;
        v = v * 3 + 1;
    }
    co_return make_blob(data);
}

} // namespace cradle
