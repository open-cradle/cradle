#ifndef CRADLE_INNER_GENERIC_LITERAL_H
#define CRADLE_INNER_GENERIC_LITERAL_H

#include <utility>

#include <cradle/inner/generic/generic.h>

namespace cradle {

/**
 * Request for a literal (immediate) value.
 *
 * Concrete class, there should be no need to subclass.
 * No caching, no introspection, making this class very simple.
 * It is serializable though.
 */
template<typename Value>
class literal_request : public abstract_request<Value>
{
 public:
    literal_request() = default;

    literal_request(Value const& value) : value_{value}
    {
    }

    literal_request(Value&& value) : value_{std::move(value)}
    {
    }

    Value
    get_value() const
    {
        return value_;
    }

    cppcoro::task<Value>
    calculate() const override
    {
        co_return value_;
    }

    template<class Archive>
    void
    serialize(Archive& ar)
    {
        ar(value_);
    }

 private:
    Value value_;
};

template<typename Value>
auto // literal_request<std::remove_cvref_t<Value>>
rq_value(Value&& value)
{
    return literal_request{std::forward<Value>(value)};
}

} // namespace cradle

#endif
