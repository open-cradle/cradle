#include <catch2/catch.hpp>

#include <cradle/inner/dll/dll_collection.h>
#include <cradle/inner/dll/dll_exceptions.h>
#include <cradle/inner/resolve/seri_registry.h>
#include <cradle/test_dlls_dir.h>

using namespace cradle;

// Covers classes dll_controller, dll_trash and dll_collection.

static char const tag[] = "[inner][dll]";

TEST_CASE("initial DLL state", tag)
{
    dll_collection the_dlls;
    auto& the_seri_registry{seri_registry::instance()};

    REQUIRE(the_dlls.size() == 0);
    REQUIRE(the_dlls.trash_size() == 0);
    // TODO assuming that all tests leave the seri_registry empty may be wrong
    REQUIRE(the_seri_registry.size() == 0);
}

TEST_CASE("load/unload/reload one DLL", tag)
{
    std::string dll_name{"test_inner_dll_v1"};
    constexpr auto dll_size{3};
    dll_collection the_dlls;
    auto& the_seri_registry{seri_registry::instance()};

    the_dlls.load(get_test_dlls_dir(), dll_name);
    REQUIRE(the_dlls.size() == 1);
    REQUIRE(the_dlls.trash_size() == 0);
    REQUIRE(the_seri_registry.size() == dll_size);

    the_dlls.unload(dll_name);
    REQUIRE(the_dlls.size() == 0);
    REQUIRE(the_dlls.trash_size() == 1);
    REQUIRE(the_seri_registry.size() == 0);

    the_dlls.load(get_test_dlls_dir(), dll_name);
    REQUIRE(the_dlls.size() == 1);
    REQUIRE(the_dlls.trash_size() == 1);
    REQUIRE(the_seri_registry.size() == dll_size);
}

TEST_CASE("load/unload/reload two DLLs", tag)
{
    std::string dll0_name{"test_inner_dll_v1"};
    constexpr auto dll0_size{3};
    std::string dll1_name{"test_inner_dll_x0"};
    constexpr auto dll1_size{1};
    dll_collection the_dlls;
    auto& the_seri_registry{seri_registry::instance()};

    the_dlls.load(get_test_dlls_dir(), dll0_name);
    REQUIRE(the_dlls.size() == 1);
    REQUIRE(the_dlls.trash_size() == 0);
    REQUIRE(the_seri_registry.size() == dll0_size);

    the_dlls.load(get_test_dlls_dir(), dll1_name);
    REQUIRE(the_dlls.size() == 2);
    REQUIRE(the_dlls.trash_size() == 0);
    REQUIRE(the_seri_registry.size() == dll0_size + dll1_size);

    the_dlls.unload(dll0_name);
    REQUIRE(the_dlls.size() == 1);
    REQUIRE(the_dlls.trash_size() == 1);
    REQUIRE(the_seri_registry.size() == dll1_size);

    the_dlls.unload(dll1_name);
    REQUIRE(the_dlls.size() == 0);
    REQUIRE(the_dlls.trash_size() == 2);
    REQUIRE(the_seri_registry.size() == 0);

    the_dlls.load(get_test_dlls_dir(), dll1_name);
    REQUIRE(the_dlls.size() == 1);
    REQUIRE(the_dlls.trash_size() == 2);
    REQUIRE(the_seri_registry.size() == dll1_size);

    the_dlls.load(get_test_dlls_dir(), dll0_name);
    REQUIRE(the_dlls.size() == 2);
    REQUIRE(the_dlls.trash_size() == 2);
    REQUIRE(the_seri_registry.size() == dll1_size + dll0_size);
}

TEST_CASE("unload DLLs with regex", tag)
{
    std::string dll_v1_name{"test_inner_dll_v1"};
    constexpr auto dll_v1_size{3};
    std::string dll_x0_name{"test_inner_dll_x0"};
    constexpr auto dll_x0_size{1};
    std::string dll_x1_name{"test_inner_dll_x1"};
    constexpr auto dll_x1_size{1};
    dll_collection the_dlls;
    auto& the_seri_registry{seri_registry::instance()};

    the_dlls.load(get_test_dlls_dir(), dll_v1_name);
    the_dlls.load(get_test_dlls_dir(), dll_x0_name);
    the_dlls.load(get_test_dlls_dir(), dll_x1_name);
    REQUIRE(the_dlls.size() == 3);
    REQUIRE(the_dlls.trash_size() == 0);
    REQUIRE(
        the_seri_registry.size() == dll_v1_size + dll_x0_size + dll_x1_size);

    the_dlls.unload("test_inner_dll_x.*");
    REQUIRE(the_dlls.size() == 1);
    REQUIRE(the_dlls.trash_size() == 2);
    REQUIRE(the_seri_registry.size() == dll_v1_size);

    the_dlls.unload("test_inner_dll_y.*");
    REQUIRE(the_dlls.size() == 1);
    REQUIRE(the_dlls.trash_size() == 2);
    REQUIRE(the_seri_registry.size() == dll_v1_size);

    the_dlls.unload("test_inner_dll_v.*");
    REQUIRE(the_dlls.size() == 0);
    REQUIRE(the_dlls.trash_size() == 3);
    REQUIRE(the_seri_registry.size() == 0);
}

TEST_CASE("loading a non-existing DLL fails", tag)
{
    std::string dll_name{"none_such"};
    dll_collection the_dlls;
    auto& the_seri_registry{seri_registry::instance()};

    REQUIRE_THROWS_AS(
        the_dlls.load(get_test_dlls_dir(), dll_name), dll_load_error);

    REQUIRE(the_dlls.size() == 0);
    REQUIRE(the_dlls.trash_size() == 0);
    REQUIRE(the_seri_registry.size() == 0);
}

TEST_CASE("loading an already loaded DLL has no effect", tag)
{
    std::string dll0_name{"test_inner_dll_x0"};
    constexpr auto dll0_size{1};
    dll_collection the_dlls;
    auto& the_seri_registry{seri_registry::instance()};

    the_dlls.load(get_test_dlls_dir(), dll0_name);
    REQUIRE(the_dlls.size() == 1);
    REQUIRE(the_dlls.trash_size() == 0);
    REQUIRE(the_seri_registry.size() == dll0_size);

    the_dlls.load(get_test_dlls_dir(), dll0_name);
    REQUIRE(the_dlls.size() == 1);
    REQUIRE(the_dlls.trash_size() == 0);
    REQUIRE(the_seri_registry.size() == dll0_size);

    the_dlls.unload(dll0_name);
    REQUIRE(the_dlls.size() == 0);
    REQUIRE(the_dlls.trash_size() == 1);
    REQUIRE(the_seri_registry.size() == 0);
}

TEST_CASE("attempt to load a DLL missing the mandatory export", tag)
{
    std::string dll_name{"test_inner_dll_missing_export"};
    dll_collection the_dlls;
    auto& the_seri_registry{seri_registry::instance()};

    REQUIRE_THROWS_AS(
        the_dlls.load(get_test_dlls_dir(), dll_name), dll_load_error);

    REQUIRE(the_dlls.size() == 0);
    REQUIRE(the_dlls.trash_size() == 0);
    REQUIRE(the_seri_registry.size() == 0);
}

TEST_CASE("attempt to load a DLL with a failing export function", tag)
{
    std::string dll_name{"test_inner_dll_failing_create_catalog"};
    dll_collection the_dlls;
    auto& the_seri_registry{seri_registry::instance()};

    REQUIRE_THROWS_AS(
        the_dlls.load(get_test_dlls_dir(), dll_name), dll_load_error);

    REQUIRE(the_dlls.size() == 0);
    REQUIRE(the_dlls.trash_size() == 0);
    REQUIRE(the_seri_registry.size() == 0);
}

TEST_CASE("attempt to unload a DLL that is not loaded", tag)
{
    std::string dll_name{"dll_that_is_not_loaded"};
    dll_collection the_dlls;

    REQUIRE_THROWS_AS(the_dlls.unload(dll_name), dll_unload_error);
}
