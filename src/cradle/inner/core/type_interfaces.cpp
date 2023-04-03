#include <cradle/inner/core/type_interfaces.h>

#include <cassert>
#include <cctype>
#include <cstring>
#include <iomanip>

#include <boost/functional/hash.hpp>

namespace cradle {

bool
operator==(blob const& a, blob const& b)
{
    return a.size() == b.size()
           && (a.data() == b.data()
               || std::memcmp(a.data(), b.data(), a.size()) == 0);
}

bool
operator<(blob const& a, blob const& b)
{
    return a.size() < b.size()
           || (a.size() == b.size() && a.data() != b.data()
               && std::memcmp(a.data(), b.data(), a.size()) < 0);
}

size_t
hash_value(blob const& x)
{
    uint8_t const* bytes = reinterpret_cast<uint8_t const*>(x.data());
    return boost::hash_range(bytes, bytes + x.size());
}

blob
make_static_blob(std::byte const* data, size_t size)
{
    return blob{data, size};
}

blob
make_string_literal_blob(char const* data)
{
    return make_static_blob(as_bytes(data), strlen(data));
}

blob
make_blob(std::string s)
{
    auto owner{std::make_shared<string_owner>(std::move(s))};
    auto data{owner->data()};
    auto size{owner->size()};
    return blob{std::move(owner), data, size};
}

blob
make_blob(byte_vector v)
{
    auto owner{std::make_shared<byte_vector_owner>(std::move(v))};
    auto bytes{owner->bytes()};
    auto size{owner->size()};
    return blob{std::move(owner), bytes, size};
}

blob
make_blob(byte_vector v, std::size_t size)
{
    assert(size <= v.size());
    auto owner{std::make_shared<byte_vector_owner>(std::move(v))};
    auto bytes{owner->bytes()};
    return blob{std::move(owner), bytes, size};
}

std::shared_ptr<byte_vector_owner>
make_shared_buffer(std::size_t size)
{
    return std::make_shared<byte_vector_owner>(byte_vector(size));
}

// TODO try to optimize for string_owner
std::string
to_string(blob const& x)
{
    return std::string{reinterpret_cast<char const*>(x.data()), x.size()};
}

// Decides whether a blob can be interpreted as a printable string
static bool
is_printable(blob const& b)
{
    if (b.size() > 1024)
    {
        return false;
    }

    for (size_t i = 0; i != b.size(); ++i)
    {
        int c = static_cast<int>(b.data()[i]);
        if (c < 0 || c > 255 || !std::isprint(c))
        {
            return false;
        }
    }

    return true;
}

static void
write_blob_range(
    std::ostream& s,
    std::byte const* data,
    std::size_t i_begin,
    std::size_t i_end)
{
    s << std::hex;
    auto prev_fill = s.fill('0');
    for (auto i = i_begin; i != i_end; ++i)
    {
        if (i != i_begin)
        {
            s << ' ';
        }
        s << std::setw(2) << static_cast<unsigned int>(data[i]);
    }
    s << std::dec;
    s.fill(prev_fill);
}

std::ostream&
operator<<(std::ostream& s, blob const& b)
{
    std::size_t size = b.size();
    std::byte const* data = b.data();
    if (size == 1)
    {
        s << "1-byte blob";
    }
    else
    {
        s << size << "-bytes blob";
    }
    if (size > 0)
    {
        s << ": ";
        if (is_printable(b))
        {
            s << std::string{
                reinterpret_cast<char const*>(b.data()), b.size()};
        }
        else if (size <= 20)
        {
            write_blob_range(s, data, 0, size);
        }
        else
        {
            write_blob_range(s, data, 0, 15);
            s << " ... ";
            write_blob_range(s, data, size - 4, size);
        }
    }
    return s;
}

} // namespace cradle
