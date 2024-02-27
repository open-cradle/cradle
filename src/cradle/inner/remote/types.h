#ifndef CRADLE_INNER_REMOTE_TYPES_H
#define CRADLE_INNER_REMOTE_TYPES_H

#include <cassert>

namespace cradle {

// Identifies a record in the memory cache on a remote.
class remote_cache_record_id
{
 public:
    // The underlying integral value
    using value_type = int64_t;

    struct first_tag
    {
    };

 private:
    static inline constexpr value_type no_value{0};
    static inline constexpr value_type first_value{1};

    value_type value_;

 public:
    // Creates an unset id
    remote_cache_record_id() : value_{no_value}
    {
    }

    // Creates a set id ready for sequencing
    explicit remote_cache_record_id(first_tag tag) : value_{first_value}
    {
    }

    // Creates an id, set or not, from an externalized value
    explicit remote_cache_record_id(value_type value) : value_{value}
    {
    }

    // Returns true if set, false for unset
    bool
    is_set()
    {
        return value_ != no_value;
    }

    // Externalizes the value
    value_type
    value() const
    {
        return value_;
    }

    // Sequences to the next id
    remote_cache_record_id
    operator++(int)
    {
        assert(is_set());
        return remote_cache_record_id{value_++};
    }
};

} // namespace cradle

#endif
