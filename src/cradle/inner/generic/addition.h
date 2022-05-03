#ifndef CRADLE_INNER_GENERIC_ADDITION_H
#define CRADLE_INNER_GENERIC_ADDITION_H

#include <cradle/inner/generic/generic.h>

#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>

namespace cradle {

class addition_request
{
 public:
    using value_type = int;
    using subtype = literal_request<value_type>;

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
        : id_(make_captured_id(234)), subrequests_(subrequests)
    {
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

    cppcoro::task<value_type>
    create_task() const;

#if 0
    template<class Archive>
    void
    serialize(Archive& ar)
    {
        // TODO cereal fails on id_ and subrequests_
        // Fix id_ by separating into save() and load()
        //ar(id_, summary_, subrequests_);
        ar(summary_, subrequests_);
    }
#endif

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
        // Create id_
    }

    std::vector<std::shared_ptr<subtype>> const&
    get_subrequests() const
    {
        return subrequests_;
    }

 private:
    captured_id id_;
    std::string summary_;
    std::vector<std::shared_ptr<subtype>> subrequests_;
};

} // namespace cradle

#endif
