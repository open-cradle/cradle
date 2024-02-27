#include <regex>

#include <catch2/catch.hpp>

#include <cradle/inner/introspection/tasklet_impl.h>
#include <cradle/inner/introspection/tasklet_info.h>
#include <cradle/rpclib/common/common.h>

using namespace cradle;

static char const tag[] = "[rpclib][common]";

TEST_CASE("make_info_tuples", tag)
{
    tasklet_impl t0{false, "pool0", "title0"};
    tasklet_impl t1{false, "pool1", "title1", &t0};

    // ~tasklet_impl() triggers an assertion if there was no on_finished();
    // so also if an exception occurs before on_finished().

    // info0 captured before on_finished(), info1 after
    tasklet_info info0{t0};
    t0.on_finished();
    t1.on_finished();
    tasklet_info info1{t1};

    tasklet_info_list infos{info0, info1};

    auto tuples = make_info_tuples(infos);
    REQUIRE(tuples.size() == 2);

    auto tup0 = tuples[0];
    REQUIRE(std::get<0>(tup0) == t0.own_id());
    REQUIRE(std::get<1>(tup0) == "pool0");
    REQUIRE(std::get<2>(tup0) == "title0");
    REQUIRE(std::get<3>(tup0) == NO_TASKLET_ID);
    auto events0 = std::get<4>(tup0);
    REQUIRE(events0.size() == 1);
    auto event00 = events0[0];
    REQUIRE(std::get<0>(event00) > 0);
    REQUIRE(std::get<1>(event00) == "scheduled");
    REQUIRE(std::get<2>(event00) == "");

    auto tup1 = tuples[1];
    REQUIRE(std::get<0>(tup1) == t1.own_id());
    REQUIRE(std::get<1>(tup1) == "pool1");
    REQUIRE(std::get<2>(tup1) == "title1");
    REQUIRE(std::get<3>(tup1) == t0.own_id());
    auto events1 = std::get<4>(tup1);
    REQUIRE(events1.size() == 2);
    auto event10 = events1[0];
    auto millis10 = std::get<0>(event10);
    REQUIRE(millis10 > 0);
    REQUIRE(std::get<1>(event10) == "scheduled");
    REQUIRE(std::get<2>(event10) == "");
    auto event11 = events1[1];
    auto millis11 = std::get<0>(event11);
    REQUIRE(millis11 >= millis10);
    REQUIRE(std::get<1>(event11) == "finished");
    REQUIRE(std::get<2>(event11) == "");
}

static tasklet_info_tuple_list
make_sample_tuple_list()
{
    tasklet_event_tuple et00{1000, "scheduled", "details00"};
    tasklet_info_tuple it0{
        12, "pool0", "title0", NO_TASKLET_ID, std::vector{et00}};
    tasklet_event_tuple et10{1001, "scheduled", "details10"};
    tasklet_event_tuple et11{1002, "running", "details11"};
    tasklet_info_tuple it1{14, "pool1", "title1", 12, std::vector{et10, et11}};
    return tasklet_info_tuple_list{it0, it1};
}

TEST_CASE("make_tasklet_infos", tag)
{
    auto infos = make_tasklet_infos(make_sample_tuple_list());

    REQUIRE(infos.size() == 2);

    auto info0 = infos[0];
    REQUIRE(info0.own_id() == 12);
    REQUIRE(info0.pool_name() == "pool0");
    REQUIRE(info0.title() == "title0");
    REQUIRE(!info0.have_client());
    auto events0 = info0.events();
    REQUIRE(events0.size() == 1);
    auto event00 = events0[0];
    REQUIRE(event00.what() == tasklet_event_type::SCHEDULED);
    REQUIRE(event00.details() == "details00");

    auto info1 = infos[1];
    REQUIRE(info1.own_id() == 14);
    REQUIRE(info1.pool_name() == "pool1");
    REQUIRE(info1.title() == "title1");
    REQUIRE(info1.have_client());
    REQUIRE(info1.client_id() == 12);
    auto events1 = info1.events();
    REQUIRE(events1.size() == 2);
    auto event10 = events1[0];
    REQUIRE(event10.when() > event00.when());
    REQUIRE(event10.what() == tasklet_event_type::SCHEDULED);
    REQUIRE(event10.details() == "details10");
    auto event11 = events1[1];
    REQUIRE(event11.when() > event10.when());
    REQUIRE(event11.what() == tasklet_event_type::RUNNING);
    REQUIRE(event11.details() == "details11");
}

// Timestamps are relative to some epoch
static std::string expected_dump{
    R"(info[0] own_id 12, pool_name pool0, title title0, client_id -
  TIME scheduled (details00)
info[1] own_id 14, pool_name pool1, title title1, client_id 12
  TIME scheduled (details10)
  TIME running (details11)
)"};

TEST_CASE("dump_tasklet_infos(tasklet_info_tuple_list)", tag)
{
    auto tuples = make_sample_tuple_list();
    std::ostringstream os;
    dump_tasklet_infos(tuples, os);
    std::regex time_re{"\\d+:\\d+:\\d+\\.\\d+"};
    auto actual = std::regex_replace(os.str(), time_re, "TIME");
    REQUIRE(actual == expected_dump);
}
