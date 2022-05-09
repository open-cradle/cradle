#ifndef CRADLE_INNER_GENERIC_GENERIC_H
#define CRADLE_INNER_GENERIC_GENERIC_H

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <cereal/types/base_class.hpp>
#include <cppcoro/shared_task.hpp>
#include <cppcoro/task.hpp>

#include <cradle/inner/core/id.h>

namespace cradle {

struct inner_service_core;

enum class caching_level_type
{
    none,
    memory,
    full
};

template<typename Value>
class abstract_request
{
 public:
    virtual ~abstract_request() = default;

    // The only thing common to literal_request and addition_request
    // This could be a wrapper for a cppcoro::shared_task
    virtual cppcoro::task<Value>
    calculate() const = 0;

#if 0
    using value_type = Value;

    static constexpr caching_level_type caching_level
        = caching_level_type::none;
    static constexpr bool introspective = false;

    /**
     * Needs to be defined only if caching_level > none
     */
    virtual captured_id const&
    get_captured_id() const = 0;

    /**
     * Needs to be defined only if introspective
     */
    virtual std::string const&
    get_summary() const = 0;

    virtual cppcoro::task<Value>
    create_task() const = 0;

    /**
     * Define the member variables to be (de-)serialized.
     * This is directly usable by cereal, also other serializers?
     * These should also be the member variables over which a hash is
     * calculated. Should cover subrequests if non-empty.
     */
    template<class Archive>
    void
    serialize(Archive& ar)
    {
        ar(summary_, subrequests_);
    }

 private:
    std::string summary_;
    std::vector<std::shared_ptr<abstract_request>> subrequests_;
#endif
};

class dont_call_exception : public std::logic_error
{
 public:
    char const*
    what() const noexcept override
    {
        return "Illegal function call";
    }
};

} // namespace cradle

#endif
