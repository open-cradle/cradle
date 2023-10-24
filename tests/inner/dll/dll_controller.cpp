#include <new>

#include <catch2/catch.hpp>

#include "../../support/inner_service.h"
#include <cradle/inner/dll/dll_controller.h>
#include <cradle/inner/dll/dll_trash_impl.h>
#include <cradle/inner/utilities/logging.h>
#include <cradle/test_dlls_dir.h>

using namespace cradle;

static char const tag[] = "[inner][dll]";

TEST_CASE("load/unload a DLL", tag)
{
    std::string const dll_name{"test_inner_dll_v1"};
    auto resources{make_inner_test_resources()};
    dll_trash_impl trash;
    auto logger{ensure_logger("dll")};

    auto the_controller{std::make_unique<dll_controller>(
        *resources, trash, *logger, get_test_dlls_dir(), dll_name)};

    REQUIRE(trash.size() == 0);

    the_controller.reset();

    REQUIRE(trash.size() == 1);
}

class throwing_dll_trash : public dll_trash
{
 public:
    void
    add(boost::dll::shared_library* lib)
    {
        num_throws_ += 1;
        throw std::bad_alloc();
    }

    std::size_t
    size() const
    {
        return 0;
    }

    int
    num_throws() const
    {
        return num_throws_;
    }

 private:
    int num_throws_{0};
};

TEST_CASE("unload a DLL where the trash object throws", tag)
{
    std::string const dll_name{"test_inner_dll_v1"};
    auto resources{make_inner_test_resources()};
    throwing_dll_trash trash;
    auto logger{ensure_logger("dll")};

    auto the_controller{std::make_unique<dll_controller>(
        *resources, trash, *logger, get_test_dlls_dir(), dll_name)};

    REQUIRE(trash.num_throws() == 0);

    REQUIRE_NOTHROW(the_controller.reset());

    REQUIRE(trash.num_throws() == 1);
}
