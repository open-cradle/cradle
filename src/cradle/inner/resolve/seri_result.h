#ifndef CRADLE_INNER_RESOLVE_SERI_RESULT_H
#define CRADLE_INNER_RESOLVE_SERI_RESULT_H

#include <memory>
#include <utility>

#include <cradle/inner/core/type_definitions.h>

namespace cradle {

// Observes the deserialization of a serialized result.
class deserialization_observer
{
 public:
    virtual ~deserialization_observer() = default;

    virtual void
    on_deserialized()
        = 0;
};

// Contains a serialized value obtained from resolving a request, and
// optionally a deserialization observer.
class serialized_result
{
 public:
    serialized_result(blob value) : value_{value}
    {
    }

    serialized_result(
        blob value, std::unique_ptr<deserialization_observer> observer)
        : value_{value}, observer_{std::move(observer)}
    {
    }

    blob
    value()
    {
        return value_;
    }

    void
    on_deserialized()
    {
        if (observer_)
        {
            observer_->on_deserialized();
        }
    }

 private:
    blob value_;
    std::unique_ptr<deserialization_observer> observer_;
};

} // namespace cradle

#endif
