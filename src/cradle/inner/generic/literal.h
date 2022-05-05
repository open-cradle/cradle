#ifndef CRADLE_INNER_GENERIC_LITERAL_H
#define CRADLE_INNER_GENERIC_LITERAL_H

#include <cradle/inner/generic/generic.h>

namespace cradle {

/**
 * Request for a literal value.
 *
 * Concrete class, there should be no need to subclass.
 * No caching, no introspection, making this class very simple.
 * It is serializable though.
 */
template<typename Value>
class literal_request : public abstract_request<Value>
{
 public:
    /**
     * Intended to be initialized by deserializer
     */
    literal_request() = default;

    literal_request(Value literal) : literal_(literal)
    {
    }

    Value
    get_literal() const
    {
        return literal_;
    }

    cppcoro::task<Value>
    calculate() const override
    {
        co_return literal_;
    }

    template<class Archive>
    void
    serialize(Archive& ar)
    {
        ar(literal_);
    }

 private:
    Value literal_;
};

} // namespace cradle

#endif
