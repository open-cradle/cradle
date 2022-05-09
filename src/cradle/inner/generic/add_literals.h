#ifndef CRADLE_INNER_GENERIC_ADD_LITERALS_H
#define CRADLE_INNER_GENERIC_ADD_LITERALS_H

#include <cradle/inner/generic/literal.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/service/core.h>

#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>

namespace cradle {

template<typename Value>
class add_literals_request
{
 public:
    using value_type = Value;
    using subtype = literal_request<Value>;

    static constexpr caching_level_type caching_level
        = caching_level_type::full;
    static constexpr bool introspective = true;

    /**
     * Object should be constructed by deserializer
     */
    add_literals_request() = default;

    add_literals_request(std::vector<Value> const& values)
        : summary_{"add_literals"}
    {
        for (auto v: values)
        {
            subrequests_.emplace_back(subtype{std::move(v)});
        }
        create_id();
    }

    cppcoro::task<Value>
    calculate() const
    {
        co_return co_await shared_task_;
    }

    captured_id const&
    get_captured_id() const
    {
        return id_;
    }

    /**
     * Needs to be defined only if introspective
     */
    std::string const&
    get_summary() const
    {
        return summary_;
    }

    cppcoro::task<Value>
    create_task() const
    {
        Value res{};
        for (auto const& subreq : subrequests_)
        {
            res += co_await subreq.calculate();
        }
        co_return res;
    }

    template<class Archive>
    void
    save(Archive& ar) const
    {
        ar(summary_, subrequests_);
    }

    template<class Archive>
    void
    load(Archive& ar)
    {
        ar(summary_, subrequests_);
        create_id();
    }

    std::vector<subtype> const&
    get_subrequests() const
    {
        return subrequests_;
    }

    void
    set_shared_task(cppcoro::shared_task<Value> const& shared_task)
    {
        shared_task_ = shared_task;
    }

 private:
    captured_id id_;
    std::string summary_;
    std::vector<subtype> subrequests_;
    cppcoro::shared_task<Value> shared_task_;

    void
    create_id()
    {
        id_ = make_captured_id(summary_);
    }
};

template<typename Value>
std::shared_ptr<add_literals_request<Value>>
make_shared_add_literals_request(
    inner_service_core& service,
    tasklet_tracker* client,
    std::vector<Value> const& values)
{
    auto shared_req{make_shared<add_literals_request<Value>>(values)};
    auto shared_task
        = make_shared_task_for_request(service, shared_req, client);
    shared_req->set_shared_task(shared_task);
    return shared_req;
}

} // namespace cradle

#endif
