#ifndef CRADLE_INNER_GENERIC_ADDITION_H
#define CRADLE_INNER_GENERIC_ADDITION_H

#include <cradle/inner/generic/generic.h>
#include <cradle/inner/introspection/tasklet.h>
#include <cradle/inner/service/core.h>

#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>

namespace cradle {

template<typename Value>
class addition_request : public abstract_request<Value>
{
 public:
    using value_type = Value;
    using subtype = abstract_request<Value>;

    static constexpr caching_level_type caching_level
        = caching_level_type::full;
    static constexpr bool introspective = true;

    /**
     * Object should be constructed by deserializer
     */
    addition_request()
    {
    }

    addition_request(std::vector<std::shared_ptr<subtype>> const& subrequests)
        : summary_{"addition"}, subrequests_(subrequests)
    {
        create_id();
    }

    cppcoro::task<Value>
    calculate() const override
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
            res += co_await subreq->calculate();
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

    std::vector<std::shared_ptr<subtype>> const&
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
    std::vector<std::shared_ptr<subtype>> subrequests_;
    cppcoro::shared_task<Value> shared_task_;

    void
    create_id()
    {
        id_ = make_captured_id(summary_);
    }
};

template<typename Value>
std::shared_ptr<addition_request<Value>>
make_shared_addition_request(
    inner_service_core& service,
    tasklet_tracker* client,
    std::vector<std::shared_ptr<abstract_request<Value>>> const& subrequests)
{
    auto shared_req{make_shared<addition_request<Value>>(subrequests)};
    auto shared_task
        = make_shared_task_for_request(service, shared_req, client);
    shared_req->set_shared_task(shared_task);
    return shared_req;
}

} // namespace cradle

#endif
