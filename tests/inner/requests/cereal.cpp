#include <memory>
#include <sstream>

#include <catch2/catch.hpp>

#include <cradle/inner/requests/cereal.h>

using namespace cradle;

namespace {

class base
{
 public:
    virtual ~base() = default;

    virtual int
    get_answer() const = 0;
};

// Cereal puts its data in global objects that are visible to all test cases.
// Different test cases use different-tagged derived classes so they don't
// interfere with each other. There is no need to tag the base class, too.

template<int tag>
class derived : public base
{
 public:
    derived(int answer, bool do_register, request_uuid const& uuid)
        : answer_(answer)
    {
        if (do_register)
        {
            register_polymorphic_type<derived, base>(uuid);
        }
    }

    int
    get_answer() const override
    {
        return answer_;
    }

 private:
    // cereal-related

    friend class cereal::access;
    derived()
    {
    }

 public:
    template<typename Archive>
    void
    serialize(Archive& ar)
    {
        ar(cereal::make_nvp("answer", answer_));
    }

 private:
    int answer_{};
};

template<int tag>
class container
{
 public:
    static inline std::string const uuid_text{"my_uuid"};
    static inline std::string const version_text{"my_version"};

    container(int answer, bool do_register = true)
        : impl_{std::make_shared<derived<tag>>(
            answer, do_register, request_uuid(uuid_text, version_text))}
    {
    }

    int
    get_answer() const
    {
        return impl_->get_answer();
    }

 public:
    // cereal-related

    container()
    {
    }

    template<typename Archive>
    void
    serialize(Archive& ar)
    {
        ar(impl_);
    }

 private:
    std::shared_ptr<base> impl_;
};

// Wraps an ar(obj) call into an expression suitable for REQUIRE_THROWS
template<typename Archive, typename Object>
void
wrap_archive(Archive& ar, Object& obj)
{
    ar(obj);
}

// register_polymorphic_type() excerpt: register the polymorphic type
// relation, but not the uuid.
template<typename T, typename Base>
void
register_polymorphic_type_no_uuid()
{
    cereal::detail::OutputBindingCreator<cereal::JSONOutputArchive, T> x;
    cereal::detail::InputBindingCreator<cereal::JSONInputArchive, T> y;
    cereal::detail::RegisterPolymorphicCaster<Base, T>::bind();
}

} // namespace

TEST_CASE("serialize a polymorphic object", "[cereal]")
{
    constexpr int tag = 0;
    container<tag> c0{42};
    REQUIRE(c0.get_answer() == 42);

    // Serialize c0
    std::stringstream ss;
    {
        cereal::JSONOutputArchive oarchive(ss);
        oarchive(c0);
    }

    std::string json = ss.str();
    // Verify that the given uuid ended up in the JSON
    REQUIRE(
        json.find("\"polymorphic_name\": \"my_uuid+my_version\"")
        != std::string::npos);
    // The answer should also be there
    REQUIRE(json.find("\"answer\": 42") != std::string::npos);

    // Deserialize into c1
    container<tag> c1;
    {
        cereal::JSONInputArchive iarchive(ss);
        iarchive(c1);
    }

    REQUIRE(c1.get_answer() == 42);
}

TEST_CASE("cannot serialize an unregistered polymorphic object", "[cereal]")
{
    constexpr int tag = 1;
    // Do not register c's derived-base relationship
    container<tag> c{42, false};
    REQUIRE(c.get_answer() == 42);

    // Try to serialize c
    std::stringstream ss;
    cereal::JSONOutputArchive oarchive(ss);
    REQUIRE_THROWS_WITH(
        wrap_archive(oarchive, c),
        Catch::StartsWith("Trying to save an unregistered polymorphic type"));
}

TEST_CASE(
    "cannot serialize a polymorphic object with unknown uuid", "[cereal]")
{
    constexpr int tag = 2;
    // Trying to register a polymorphic relationship with cereal will fail
    // if no corresponding name was defined under which "derived" objects
    // should be serialized.
    REQUIRE_THROWS_WITH(
        (register_polymorphic_type_no_uuid<derived<tag>, base>()),
        Catch::StartsWith("uuid_registry has no entry for"));
}

TEST_CASE("cannot use the same uuid for different types", "[cereal]")
{
    request_uuid uuid{"uuid_tags_3_and_4"};
    constexpr int tag3 = 3;
    constexpr int tag4 = 4;

    register_polymorphic_type<derived<tag3>, base>(uuid);
    REQUIRE_THROWS_WITH(
        (register_polymorphic_type<derived<tag4>, base>(uuid)),
        Catch::Matches(".*uuid.+refers to.+derived<3>.+and.+derived<4>"));
}

TEST_CASE("reusing a uuid for the same type", "[cereal]")
{
    request_uuid uuid{"uuid_tag_5"};
    constexpr int tag = 5;

    register_polymorphic_type<derived<tag>, base>(uuid);
    register_polymorphic_type<derived<tag>, base>(uuid);
}
