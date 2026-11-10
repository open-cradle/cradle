#include <cradle/inner/pool/job_record.h>

#include <string>

#include <catch2/catch.hpp>

#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/storage/digest.h>

using namespace cradle;

static char const tag[] = "[unit][inner][pool][job_record]";

TEST_CASE("job_record round-trips byte-faithfully", tag)
{
    SECTION("succeeded record with output digest, empty error")
    {
        job_record original;
        original.id = "job-abc-123";
        original.key = "leaf/key/succeeded";
        original.pool_name = "local-pool";
        original.status = job_status::succeeded;
        original.output_digest = "sha256:deadbeef";
        original.error = "";

        mutable_value bytes = serialize_job_record(original);
        job_record decoded = deserialize_job_record(bytes);

        REQUIRE(decoded.id == original.id);
        REQUIRE(decoded.key == original.key);
        REQUIRE(decoded.pool_name == original.pool_name);
        REQUIRE(decoded.status == original.status);
        REQUIRE(decoded.output_digest == original.output_digest);
        REQUIRE(decoded.error == original.error);

        // Re-encoding the decoded record yields identical bytes.
        REQUIRE(serialize_job_record(decoded) == bytes);
    }

    SECTION("failed record with error, empty output digest")
    {
        job_record original;
        original.id = "job-def-456";
        original.key = "leaf/key/failed";
        original.pool_name = "local-pool";
        original.status = job_status::failed;
        original.output_digest = "";
        original.error = "provider threw std::runtime_error";

        mutable_value bytes = serialize_job_record(original);
        job_record decoded = deserialize_job_record(bytes);

        REQUIRE(decoded.id == original.id);
        REQUIRE(decoded.key == original.key);
        REQUIRE(decoded.pool_name == original.pool_name);
        REQUIRE(decoded.status == original.status);
        REQUIRE(decoded.output_digest == original.output_digest);
        REQUIRE(decoded.error == original.error);

        REQUIRE(serialize_job_record(decoded) == bytes);
    }

    SECTION("queued record with both output digest and error empty")
    {
        job_record original;
        original.id = "job-ghi-789";
        original.key = "leaf/key/queued";
        original.pool_name = "local-pool";
        original.status = job_status::queued;
        original.output_digest = "";
        original.error = "";

        mutable_value bytes = serialize_job_record(original);
        job_record decoded = deserialize_job_record(bytes);

        REQUIRE(decoded.id == original.id);
        REQUIRE(decoded.key == original.key);
        REQUIRE(decoded.pool_name == original.pool_name);
        REQUIRE(decoded.status == original.status);
        REQUIRE(decoded.output_digest == original.output_digest);
        REQUIRE(decoded.error == original.error);

        REQUIRE(serialize_job_record(decoded) == bytes);
    }
}

TEST_CASE("is_terminal reflects the lifecycle states", tag)
{
    SECTION("terminal states")
    {
        REQUIRE(is_terminal(job_status::succeeded));
        REQUIRE(is_terminal(job_status::failed));
        REQUIRE(is_terminal(job_status::cancelled));
    }

    SECTION("non-terminal states")
    {
        REQUIRE_FALSE(is_terminal(job_status::queued));
        REQUIRE_FALSE(is_terminal(job_status::running));
    }
}

TEST_CASE("job_record_key prefixes the id with jobs/", tag)
{
    REQUIRE(job_record_key("abc") == "jobs/abc");
    REQUIRE(job_record_key("") == "jobs/");
    REQUIRE(job_record_key("job-def-456") == "jobs/job-def-456");
}

TEST_CASE("deserialize_job_record throws on malformed bytes", tag)
{
    SECTION("empty bytes")
    {
        mutable_value value = make_blob(std::string{});
        REQUIRE_THROWS(deserialize_job_record(value));
    }

    SECTION("well-formed msgpack of the wrong shape (not an array)")
    {
        // 0x01 is a msgpack positive fixint, a valid value but not an array.
        std::string bytes;
        bytes.push_back(static_cast<char>(0x01));
        mutable_value value = make_blob(bytes);
        REQUIRE_THROWS(deserialize_job_record(value));
    }

    SECTION("msgpack array of the wrong length")
    {
        // 0x93 is a msgpack fixarray of 3 elements, followed by three fixints.
        std::string bytes;
        bytes.push_back(static_cast<char>(0x93));
        bytes.push_back(static_cast<char>(0x01));
        bytes.push_back(static_cast<char>(0x02));
        bytes.push_back(static_cast<char>(0x03));
        mutable_value value = make_blob(bytes);
        REQUIRE_THROWS(deserialize_job_record(value));
    }

    SECTION("arbitrary non-msgpack bytes")
    {
        // 0xc1 is the msgpack "never used" byte; decoding must fault.
        std::string bytes;
        bytes.push_back(static_cast<char>(0xc1));
        bytes.push_back(static_cast<char>(0xff));
        bytes.push_back(static_cast<char>(0x00));
        mutable_value value = make_blob(bytes);
        REQUIRE_THROWS(deserialize_job_record(value));
    }
}
