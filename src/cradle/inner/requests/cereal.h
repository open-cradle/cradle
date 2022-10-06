#ifndef CRADLE_INNER_REQUESTS_CEREAL_H
#define CRADLE_INNER_REQUESTS_CEREAL_H

/*
 * An alternative way of registering polymorphic types for cereal.
 * The mechanism that cereal offers (CEREAL_REGISTER_TYPE and
 * CEREAL_REGISTER_POLYMORPHIC_RELATION) is very inconvenient for
 * template types. In particular, a user would have to register
 * type-erased classes that are otherwise transparent.
 *
 * The alternative interface lets polymorphic classes register
 * themselves: they know their own type, and the interface class
 * they derive from. This happens at runtime (in the class's
 * constructor), in contrast to the initialization-time registry
 * used by the standard mechanism (based on static objects).
 *
 * The downside is that polymorphic classes can be deserialized only
 * if an instance was created before. (The same goes for serializing
 * these objects, but that is not really a limitation.)
 */

#include <iostream>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

#include <cereal/archives/json.hpp>
#include <cereal/details/util.hpp>
#include <cereal/types/polymorphic.hpp>

#include <cradle/inner/requests/uuid.h>

namespace cradle {

/*
 * Registry of a one-to-one relationship between uuid's and C++ classes.
 * A class is identified by one or more type_index values; there could be more
 * than one type_index value for a class, but if so, all of them would compare
 * equal. A uuid referring to multiple type_index values that do not compare
 * equal is an error, and caught by the implementation.
 */
class uuid_registry
{
 public:
    // Returns the singleton
    static uuid_registry&
    instance()
    {
        return instance_;
    }

    // Adds a (uuid, type_index) pair to the registry
    void
    add(request_uuid const& uuid, std::type_index key);

    // Returns the uuid that was registered with a given type_index value.
    // Throws if no such uuid was registered.
    request_uuid const&
    find(std::type_index key);

 private:
    static uuid_registry instance_;
    std::unordered_map<std::type_index, request_uuid> map_;
    std::unordered_map<request_uuid, std::type_index, hash_request_uuid>
        inverse_map_;

    void
    add_to_map(request_uuid const& uuid, std::type_index key);
    void
    add_to_inverse_map(request_uuid const& uuid, std::type_index key);
};

// Registers T as a polymorphic class derived from Base.
// T is uniquely identified by uuid.
// Assumes any serialization will be from/to JSON.
template<typename T, typename Base>
void
register_polymorphic_type(request_uuid const& uuid)
{
    uuid_registry::instance().add(uuid, std::type_index(typeid(T)));
    cereal::detail::OutputBindingCreator<cereal::JSONOutputArchive, T> x;
    cereal::detail::InputBindingCreator<cereal::JSONInputArchive, T> y;
    cereal::detail::RegisterPolymorphicCaster<Base, T>::bind();
}

} // namespace cradle

namespace cereal {
namespace detail {

// Retrieves the name under which cereal will serialize objects of type T,
// registered with the above mechanism; this will be T's uuid.
//
// Without the requires, this definition would conflict with cereal's default
// one.
// The binding_name instance created for a concrete type passed to
// CEREAL_REGISTER_TYPE will be preferred to this one, so no attempt will be
// made to look up something in the uuid_registry that wasn't put there,
// assuming the type was registered using either mechanism.
template<typename T>
    requires(std::is_class_v<T>)
struct binding_name<T>
{
    static char const*
    name()
    {
        return cradle::uuid_registry::instance()
            .find(std::type_index(typeid(T)))
            .str()
            .c_str();
    }
};

} // namespace detail
} // namespace cereal

#endif
