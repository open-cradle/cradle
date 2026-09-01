#include <cradle/inner/core/unique_hash.h>

#include <stdexcept>
#include <utility>

namespace cradle {

namespace {

[[noreturn]] void
throw_hasher_error(char const* what)
{
    throw std::runtime_error(what);
}

} // namespace

void
unique_hasher::init()
{
    ctx_ = EVP_MD_CTX_new();
    if (ctx_ == nullptr
        || EVP_DigestInit_ex(ctx_, EVP_sha256(), nullptr) != 1)
    {
        EVP_MD_CTX_free(ctx_);
        ctx_ = nullptr;
        throw_hasher_error("failed to initialize SHA-256 hasher");
    }
}

unique_hasher::unique_hasher()
{
    init();
}

unique_hasher::~unique_hasher()
{
    EVP_MD_CTX_free(ctx_);
}

unique_hasher::unique_hasher(unique_hasher const& other)
    : result_{other.result_}, finished_{other.finished_}
{
    ctx_ = EVP_MD_CTX_new();
    if (ctx_ == nullptr || EVP_MD_CTX_copy_ex(ctx_, other.ctx_) != 1)
    {
        EVP_MD_CTX_free(ctx_);
        ctx_ = nullptr;
        throw_hasher_error("failed to copy SHA-256 hasher");
    }
}

unique_hasher&
unique_hasher::operator=(unique_hasher const& other)
{
    if (this != &other)
    {
        unique_hasher tmp(other);
        *this = std::move(tmp);
    }
    return *this;
}

unique_hasher::unique_hasher(unique_hasher&& other) noexcept
    : ctx_{other.ctx_},
      result_{other.result_},
      finished_{other.finished_}
{
    other.ctx_ = nullptr;
}

unique_hasher&
unique_hasher::operator=(unique_hasher&& other) noexcept
{
    if (this != &other)
    {
        EVP_MD_CTX_free(ctx_);
        ctx_ = other.ctx_;
        result_ = other.result_;
        finished_ = other.finished_;
        other.ctx_ = nullptr;
    }
    return *this;
}

std::string
unique_hasher::get_string()
{
    finish();

    // This low-level code is much (say, 40x) faster than an implementation
    // based on std::ostringstream or fmt::format.
    std::string s(result_size * 2, '?');
    char* p = s.data();
    // Clang completely unrolls this loop.
    for (std::size_t i = 0; i < result_size; ++i)
    {
        static const char x[] = "0123456789abcdef";
        uint8_t val = result_.data()[i];
        *p++ = x[val >> 4];
        *p++ = x[val & 0xf];
    }
    return s;
}

void
unique_hasher::finish()
{
    if (!finished_)
    {
        unsigned int digest_len = 0;
        if (EVP_DigestFinal_ex(ctx_, result_.data(), &digest_len) != 1
            || digest_len != result_size)
        {
            throw_hasher_error("failed to finalize SHA-256 hasher");
        }
        finished_ = true;
    }
}

void
update_unique_hash(unique_hasher& hasher, std::string const& val)
{
    hasher.encode_bytes(val.data(), val.size());
}

void
update_unique_hash(unique_hasher& hasher, char const* val)
{
    hasher.encode_bytes(val, std::strlen(val));
}

void
update_unique_hash(unique_hasher& hasher, blob const& val)
{
    // A tag byte is used to distinguish between:
    // - A plain blob, where the hash is calculated over the blob data.
    // - A blob file, where the hash is calculated over the file path.
    // Without the tag, a hash over a plain blob containing something that
    // looks like a file path might be equal to the hash over a blob file.
    if (auto const* owner = val.mapped_file_data_owner())
    {
        update_unique_hash(hasher, uint8_t{0x01});
        auto path{owner->mapped_file()};
        hasher.encode_bytes(path.data(), path.size());
    }
    else
    {
        update_unique_hash(hasher, uint8_t{0x00});
        hasher.encode_bytes(val.data(), val.size());
    }
}

void
update_unique_hash(unique_hasher& hasher, byte_vector const& val)
{
    hasher.encode_bytes(val.data(), val.size());
}

} // namespace cradle
