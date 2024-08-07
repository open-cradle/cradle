#include <catch2/catch.hpp>

#include "../../support/inner_service.h"
#include <cradle/inner/dll/dll_collection.h>
#include <cradle/inner/dll/dll_exceptions.h>
#include <cradle/inner/resolve/seri_registry.h>
#include <cradle/test_dlls_dir.h>

using namespace cradle;

// Covers classes dll_controller, dll_trash and dll_collection.
// The dll_collection objects tested here are not the ones owned by the
// inner_resources objects. In contrast, the DLLs' requests are being
// registered in the seri_registry instances owned by the inner_resources
// objects.

static char const tag[] = "[inner][dll]";

TEST_CASE("initial DLL state", tag)
{
    auto resources{make_inner_test_resources()};
    dll_collection the_dlls{*resources};
    auto the_seri_registry{resources->get_seri_registry()};

    REQUIRE(the_dlls.size() == 0);
    REQUIRE(the_dlls.trash_size() == 0);
    REQUIRE(the_seri_registry->size() == 0);
}

TEST_CASE("load/unload/reload one DLL", tag)
{
    auto resources{make_inner_test_resources()};
    dll_collection the_dlls{*resources};
    auto the_seri_registry{resources->get_seri_registry()};
    std::string dll_name{"test_inner_dll_v1"};
    constexpr auto dll_size{6};

    the_dlls.load(get_test_dlls_dir(), dll_name);
    REQUIRE(the_dlls.size() == 1);
    REQUIRE(the_dlls.trash_size() == 0);
    REQUIRE(the_seri_registry->size() == dll_size);

    the_dlls.unload(dll_name);
    REQUIRE(the_dlls.size() == 0);
    REQUIRE(the_dlls.trash_size() == 1);
    REQUIRE(the_seri_registry->size() == 0);

    the_dlls.load(get_test_dlls_dir(), dll_name);
    REQUIRE(the_dlls.size() == 1);
    REQUIRE(the_dlls.trash_size() == 1);
    REQUIRE(the_seri_registry->size() == dll_size);
}

TEST_CASE("load/unload/reload two DLLs", tag)
{
    auto resources{make_inner_test_resources()};
    dll_collection the_dlls{*resources};
    auto the_seri_registry{resources->get_seri_registry()};
    std::string dll0_name{"test_inner_dll_v1"};
    constexpr auto dll0_size{6};
    std::string dll1_name{"test_inner_dll_x0"};
    constexpr auto dll1_size{1};

    the_dlls.load(get_test_dlls_dir(), dll0_name);
    REQUIRE(the_dlls.size() == 1);
    REQUIRE(the_dlls.trash_size() == 0);
    REQUIRE(the_seri_registry->size() == dll0_size);

    the_dlls.load(get_test_dlls_dir(), dll1_name);
    REQUIRE(the_dlls.size() == 2);
    REQUIRE(the_dlls.trash_size() == 0);
    REQUIRE(the_seri_registry->size() == dll0_size + dll1_size);

    the_dlls.unload(dll0_name);
    REQUIRE(the_dlls.size() == 1);
    REQUIRE(the_dlls.trash_size() == 1);
    REQUIRE(the_seri_registry->size() == dll1_size);

    the_dlls.unload(dll1_name);
    REQUIRE(the_dlls.size() == 0);
    REQUIRE(the_dlls.trash_size() == 2);
    REQUIRE(the_seri_registry->size() == 0);

    the_dlls.load(get_test_dlls_dir(), dll1_name);
    REQUIRE(the_dlls.size() == 1);
    REQUIRE(the_dlls.trash_size() == 2);
    REQUIRE(the_seri_registry->size() == dll1_size);

    the_dlls.load(get_test_dlls_dir(), dll0_name);
    REQUIRE(the_dlls.size() == 2);
    REQUIRE(the_dlls.trash_size() == 2);
    REQUIRE(the_seri_registry->size() == dll1_size + dll0_size);
}

TEST_CASE("loading a non-existing DLL fails", tag)
{
    auto resources{make_inner_test_resources()};
    dll_collection the_dlls{*resources};
    std::string dll_name{"none_such"};

    // The error message is system-dependent.
    REQUIRE_THROWS_AS(
        the_dlls.load(get_test_dlls_dir(), dll_name), dll_load_error);
}

TEST_CASE("loading an already loaded DLL has no effect", tag)
{
    auto resources{make_inner_test_resources()};
    dll_collection the_dlls{*resources};
    auto the_seri_registry{resources->get_seri_registry()};
    std::string dll0_name{"test_inner_dll_x0"};
    constexpr auto dll0_size{1};

    the_dlls.load(get_test_dlls_dir(), dll0_name);
    REQUIRE(the_dlls.size() == 1);
    REQUIRE(the_dlls.trash_size() == 0);
    REQUIRE(the_seri_registry->size() == dll0_size);

    the_dlls.load(get_test_dlls_dir(), dll0_name);
    REQUIRE(the_dlls.size() == 1);
    REQUIRE(the_dlls.trash_size() == 0);
    REQUIRE(the_seri_registry->size() == dll0_size);

    the_dlls.unload(dll0_name);
    REQUIRE(the_dlls.size() == 0);
    REQUIRE(the_dlls.trash_size() == 1);
    REQUIRE(the_seri_registry->size() == 0);
}

TEST_CASE("attempt to load a DLL missing the mandatory export", tag)
{
    auto resources{make_inner_test_resources()};
    dll_collection the_dlls{*resources};
    std::string dll_name{"test_inner_dll_missing_export"};

    REQUIRE_THROWS_WITH(
        the_dlls.load(get_test_dlls_dir(), dll_name),
        Catch::Matchers::Contains(
            "cannot get symbol CRADLE_get_capabilities"));
}

TEST_CASE("attempt to load a DLL returning a null capabilities pointer", tag)
{
    auto resources{make_inner_test_resources()};
    dll_collection the_dlls{*resources};
    std::string dll_name{"test_inner_dll_null_capabilities"};

    REQUIRE_THROWS_WITH(
        the_dlls.load(get_test_dlls_dir(), dll_name),
        Catch::Matchers::Contains("get_caps_func() returned nullptr"));
}

TEST_CASE("load a DLL that does not have a seri_catalog", tag)
{
    auto resources{make_inner_test_resources()};
    dll_collection the_dlls{*resources};
    auto the_seri_registry{resources->get_seri_registry()};
    std::string dll_name{"test_inner_dll_no_create_catalog"};

    the_dlls.load(get_test_dlls_dir(), dll_name);

    REQUIRE(the_dlls.size() == 1);
    REQUIRE(the_dlls.trash_size() == 0);
    REQUIRE(the_seri_registry->size() == 0);
}

TEST_CASE("attempt to load a DLL with a failing create_catalog function", tag)
{
    auto resources{make_inner_test_resources()};
    dll_collection the_dlls{*resources};
    std::string dll_name{"test_inner_dll_failing_create_catalog"};

    REQUIRE_THROWS_WITH(
        the_dlls.load(get_test_dlls_dir(), dll_name),
        Catch::Matchers::Contains("create_catalog_func() returned nullptr"));
}

TEST_CASE("attempt to unload a DLL that is not loaded", tag)
{
    auto resources{make_inner_test_resources()};
    dll_collection the_dlls{*resources};
    std::string dll_name{"dll_that_is_not_loaded"};

    REQUIRE_THROWS_WITH(
        the_dlls.unload(dll_name),
        Catch::Matchers::Contains(
            "no DLL loaded named dll_that_is_not_loaded"));
}
