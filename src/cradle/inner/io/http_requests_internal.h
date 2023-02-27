#ifndef CRADLE_INNER_IO_HTTP_REQUESTS_INTERNAL_H
#define CRADLE_INNER_IO_HTTP_REQUESTS_INTERNAL_H

#include <memory>
#include <utility>

namespace cradle {

typedef std::unique_ptr<char, decltype(&free)> malloc_buffer_ptr;

class malloc_buffer_ptr_wrapper : public data_owner
{
 public:
    malloc_buffer_ptr_wrapper(malloc_buffer_ptr value)
        : value_{std::move(value)}
    {
    }

    ~malloc_buffer_ptr_wrapper() = default;

 private:
    malloc_buffer_ptr value_;
};

} // namespace cradle

#endif
