#ifndef CRADLE_THINKNODE_ISS_IMPL_H
#define CRADLE_THINKNODE_ISS_IMPL_H

#include <cppcoro/task.hpp>
#include <cradle/inner/generic/generic.h>
#include <cradle/typing/service/core.h>

namespace cradle {

class my_post_iss_object_request
{
 public:
    using value_type = std::string;

    static constexpr caching_level_type caching_level
        = caching_level_type::full;
    static constexpr bool introspective = true;

    // Serializing this class is problematic due to:
    // - thinknode_request_context::service_core& service (reference)
    // - thinknode_request_context::tasklet_tracker* tasklet (pointer)
    // - thinknode_type_info schema_ (api(union))
    // - blob object_data_ (should be solvable)
    // Without serialization possibilities, there is no need for:
    // - Default constructor
    // - serialize(), save(), load()
    static constexpr bool serializable = false;

    my_post_iss_object_request(
        std::shared_ptr<thinknode_request_context> const& ctx,
        string context_id,
        thinknode_type_info schema,
        blob object_data);

    captured_id const&
    get_captured_id() const
    {
        return id_;
    }

    std::string const&
    get_summary() const
    {
        return summary_;
    }

    cppcoro::task<value_type>
    create_task() const;

 public:
    std::shared_ptr<thinknode_request_context> ctx_;
    string context_id_;
    thinknode_type_info schema_;
    blob object_data_;

 private:
    std::string summary_{"post_iss_object"};
    captured_id id_;

    void
    create_id();
};

} // namespace cradle

#endif
