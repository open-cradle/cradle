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

// A request (concept) expressed as an abstract base class.
// Creating derived classes may not be possible, e.g. it looks like cereal
// cannot handle template base classes. So this is more like a concept. If
// there are subrequests then, unless all value types are identical, Value
// should be a kind of Variant / Any / dynamic.
template<typename Value>
class abstract_request
{
 public:
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

/**
 * Request for a literal value.
 *
 * Concrete class, there should be no need to subclass.
 * Creating a shared_task for literal values may imply too much overhead.
 */
template<typename Value>
class literal_request
{
 public:
    using value_type = Value;

    static constexpr caching_level_type caching_level{
        caching_level_type::none};
    static constexpr bool introspective{false};

    /**
     * Creates an uninitialized object
     */
    literal_request() = default;

    literal_request(Value literal) : literal_(literal)
    {
    }

    cppcoro::task<Value>
    create_task() const
    {
        auto task_function
            = [](literal_request const& req) -> cppcoro::task<Value> {
            co_return req.get_literal();
        };
        return task_function(*this);
    }

    template<class Archive>
    void
    serialize(Archive& ar)
    {
        ar(literal_);
    }

    Value const&
    get_literal() const
    {
        return literal_;
    }

 private:
    Value literal_;
};

} // namespace cradle

#endif
