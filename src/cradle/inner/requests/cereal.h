#ifndef CRADLE_INNER_REQUESTS_CEREAL_H
#define CRADLE_INNER_REQUESTS_CEREAL_H

// (The inner core of) CRADLE relies on cereal for its serialization:
// - Storing/loading values to/from the disk cache
// - Calculating request hashes
// - (Planned) Persisting requests

#include <cereal/cereal.hpp>

#include <cradle/inner/core/exception.h>
#include <cradle/inner/core/id.h>
#include <cradle/inner/core/type_definitions.h>

namespace cereal {

template<typename Archive>
void
save(Archive& ar, cradle::blob const& value)
{
    ar(make_size_tag(static_cast<size_type>(value.size())));
    ar(binary_data(value.data(), value.size()));
}

template<typename Archive>
void
load(Archive& ar, cradle::blob& value)
{
    throw cradle::not_implemented_error("blob cannot replace its contents");
}

template<typename Archive>
void
save(Archive& ar, cradle::captured_id const& value)
{
    // TODO we shouldn't be serializing captured_id's
    ar(make_size_tag(static_cast<size_type>(value.hash())));
}

template<typename Archive>
void
load(Archive& ar, cradle::captured_id& value)
{
    throw cradle::not_implemented_error("TODO");
}

} // namespace cereal

#endif
