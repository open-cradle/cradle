#ifndef CRADLE_INNER_RESOLVE_SERI_RESULT_H
#define CRADLE_INNER_RESOLVE_SERI_RESULT_H

#include <memory>
#include <utility>

#include <cradle/inner/core/type_definitions.h>
#include <cradle/inner/remote/types.h>

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

// Result from resolving a request to a serialized value, consisting of:
// - The serialized value itself;
// - Optionally a deserialization observer;
// - An identification of the memory cache record, if any, that was locked
//   when resolving the request.
class serialized_result
{
 public:
    serialized_result(blob value, remote_cache_record_id record_id)
        : value_{value}, record_id_{record_id}
    {
    }

    serialized_result(
        blob value,
        std::unique_ptr<deserialization_observer> observer,
        remote_cache_record_id record_id)
        : value_{value}, observer_{std::move(observer)}, record_id_{record_id}
    {
    }

    blob
    value()
    {
        return value_;
    }

    remote_cache_record_id
    get_cache_record_id() const
    {
        return record_id_;
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
    remote_cache_record_id record_id_;
};

} // namespace cradle

#endif
