# Define an object library containg the test runner for the Catch2-based tests.
# (An object library is a CMake artifact. The linker will see the object files, not a library.)
add_library(catch2_runner OBJECT
    runner.cpp)
target_link_libraries(catch2_runner PUBLIC
    cradle_inner
    Catch2::Catch2
    spdlog::spdlog)

# Don't use precompiled headers for the test runner since it needs to include
# Catch specially.
set_source_files_properties(
    runner.cpp
    PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON)

# Define a library containing basic test support.
add_library(basic_test_support
    support/common.cpp
    support/inner_service.cpp)
add_dependencies(basic_test_support
    deploy_dir_file_target)
target_link_libraries(basic_test_support PUBLIC
    cradle_inner
    cradle_plugins_inner
    Catch2::Catch2
    spdlog::spdlog)

# Define a library containing Thinknode test support.
add_library(thinknode_test_support
    support/thinknode.cpp)
target_include_directories(thinknode_test_support PRIVATE
    ${generated_src_dir})
target_link_libraries(thinknode_test_support PUBLIC
    basic_test_support
    cradle_thinknode)

# Define an object library covering the source files in unit/.
file(GLOB_RECURSE test_srcs_unit CONFIGURE_DEPENDS
    "unit/*.cpp")
add_library(test_lib_unit OBJECT ${test_srcs_unit})
add_dependencies(test_lib_unit
    thinknode_dlls_dir_target)
target_link_libraries(test_lib_unit PUBLIC
    cradle_websocket
    cradle_plugins_typing
    cradle_plugins_inner)

# Add the unit test runner.
add_executable(unit_test_runner)
target_include_directories(unit_test_runner PUBLIC
    ${generated_src_dir}
    src)
target_link_libraries(unit_test_runner PRIVATE
    catch2_runner
    thinknode_test_support
    test_lib_unit
    cradle_inner)
add_dependencies(unit_test_runner
    copy_thinknode_dlls)

# Add the unit testing target.
add_custom_target(
    unit_tests
    # Create a fresh 'unit-testing' directory within the build dir and run the
    # tests within that. (Some of them perform file I/O.)
    COMMAND ${CMAKE_COMMAND} -E remove_directory unit-testing
    COMMAND ${CMAKE_COMMAND} -E make_directory unit-testing
    COMMAND ${CMAKE_COMMAND} -E chdir unit-testing ${CMAKE_COMMAND} -E env CRADLE_DEPLOY_DIR=${CMAKE_CURRENT_BINARY_DIR} CRADLE_THINKNODE_API_TOKEN=${API_TOKEN} CRADLE_THINKNODE_DOCKER_AUTH=${DOCKER_AUTH} $<TARGET_FILE:unit_test_runner>
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    DEPENDS unit_test_runner)
add_test(
    NAME unit_tests
    COMMAND ${CMAKE_COMMAND}
        --build ${CMAKE_BINARY_DIR}
        --target unit_tests)

# Create a shared library that can be loaded dynamically at runtime.
# Used in inner/resolve/resolve_proxy.cpp.
file(GLOB_RECURSE srcs_inner_dll_v1 CONFIGURE_DEPENDS
    "inner-dll/v1/*.cpp")
add_library(test_inner_dll_v1 MODULE ${srcs_inner_dll_v1})
target_link_libraries(test_inner_dll_v1 PUBLIC
    cradle_inner)

# Create another DLL.
# Used in inner/resolve/resolve_proxy.cpp.
file(GLOB_RECURSE srcs_inner_dll_x0 CONFIGURE_DEPENDS
    "inner-dll/x0/*.cpp")
add_library(test_inner_dll_x0 MODULE ${srcs_inner_dll_x0})
target_link_libraries(test_inner_dll_x0 PUBLIC
    cradle_inner)

# Create another DLL.
# Used in inner/resolve/resolve_proxy.cpp.
file(GLOB_RECURSE srcs_inner_dll_x1 CONFIGURE_DEPENDS
    "inner-dll/x1/*.cpp")
add_library(test_inner_dll_x1 MODULE ${srcs_inner_dll_x1})
target_link_libraries(test_inner_dll_x1 PUBLIC
    cradle_inner)

# Create another DLL, containing the resolvers from x0 and x1.
# Used in inner/resolve/resolve_proxy.cpp.
set(srcs_inner_dll_x0x1
    "inner-dll/x0/adder_x0_impl.cpp"
    "inner-dll/x1/multiplier_x1_impl.cpp"
    "inner-dll/x0x1/test_seri_catalog_x0x1.cpp")
add_library(test_inner_dll_x0x1 MODULE ${srcs_inner_dll_x0x1})
target_link_libraries(test_inner_dll_x0x1 PUBLIC
    cradle_inner)

# Create an invalid DLL, that does not export the mandatory CRADLE_get_capabilities function.
file(GLOB_RECURSE srcs_inner_dll_missing_export CONFIGURE_DEPENDS
    "inner-dll/missing_export/*.cpp")
add_library(test_inner_dll_missing_export MODULE ${srcs_inner_dll_missing_export})
target_link_libraries(test_inner_dll_missing_export PUBLIC
    cradle_inner)

# Create an invalid DLL, that exports a CRADLE_get_capabilities function returning nullptr.
file(GLOB_RECURSE srcs_inner_dll_null_capabilities CONFIGURE_DEPENDS
    "inner-dll/null_capabilities/*.cpp")
add_library(test_inner_dll_null_capabilities MODULE ${srcs_inner_dll_null_capabilities})
target_link_libraries(test_inner_dll_null_capabilities PUBLIC
    cradle_inner)

# Create a DLL that does not export a create_seri_catalog function.
file(GLOB_RECURSE srcs_inner_dll_no_create_catalog CONFIGURE_DEPENDS
    "inner-dll/no_create_catalog/*.cpp")
add_library(test_inner_dll_no_create_catalog MODULE ${srcs_inner_dll_no_create_catalog})
target_link_libraries(test_inner_dll_no_create_catalog PUBLIC
    cradle_inner)

# Create a DLL exporting a create_seri_catalog function returning nullptr.
file(GLOB_RECURSE srcs_inner_dll_failing_create_catalog CONFIGURE_DEPENDS
    "inner-dll/failing_create_catalog/*.cpp")
add_library(test_inner_dll_failing_create_catalog MODULE ${srcs_inner_dll_failing_create_catalog})
target_link_libraries(test_inner_dll_failing_create_catalog PUBLIC
    cradle_inner)

# Create a C++ header file defining the location of the dynamically-loaded test DLLs.
set(test_dlls_dir_file "${intgen_src_dir}/cradle/test_dlls_dir.h")
add_custom_command(
    OUTPUT ${test_dlls_dir_file}
    COMMAND ${CMAKE_COMMAND}
        -D input_file=${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_dlls_dir.h.in
        -D output_file=${test_dlls_dir_file}
        -D test_dlls_dir=$<TARGET_FILE_DIR:test_inner_dll_v1>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_dlls_dir.cmake
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_dlls_dir.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_dlls_dir.h.in)
add_custom_target(test_dlls_dir_target DEPENDS ${test_dlls_dir_file})

# Define an object library covering the source files in inner/.
file(GLOB_RECURSE test_srcs_inner CONFIGURE_DEPENDS
    "inner/*.cpp"
    "rpclib/*.cpp")
add_library(test_lib_inner OBJECT ${test_srcs_inner})
target_link_libraries(test_lib_inner PUBLIC
    cradle_plugins_inner
    cradle_rpclib_client
    cradle_inner
    Catch2::Catch2)
add_dependencies(test_lib_inner 
    test_dlls_dir_target)

# Add the inner test runner.
add_executable(inner_test_runner)
target_link_libraries(inner_test_runner PRIVATE
    catch2_runner
    basic_test_support
    test_lib_inner)

# Add the inner testing target.
add_custom_target(
    inner_tests
    COMMAND $<TARGET_FILE:inner_test_runner>
    DEPENDS inner_test_runner)
add_dependencies(inner_tests
    rpclib_server
    copy_cradle_dlls
    test_inner_dll_v1
    test_inner_dll_x0
    test_inner_dll_x1
    test_inner_dll_x0x1)
add_test(
    NAME inner_tests
    COMMAND ${CMAKE_COMMAND}
        --build ${CMAKE_BINARY_DIR}
        --target inner_tests)

# Define an object library covering the source files in external/.
file(GLOB_RECURSE test_srcs_external CONFIGURE_DEPENDS
    "external/*.cpp")
add_library(test_lib_external OBJECT ${test_srcs_external})
target_link_libraries(test_lib_external PUBLIC
    cradle_external
    cradle_plugins_inner)

# Add the external test runner.
add_executable(external_test_runner)
target_include_directories(external_test_runner PUBLIC
    ${generated_src_dir}
    src)
target_link_libraries(external_test_runner PRIVATE
    catch2_runner
    thinknode_test_support
    test_lib_external)

# Add the external testing target.
add_custom_target(
    external_tests
    COMMAND $<TARGET_FILE:external_test_runner>
    DEPENDS external_test_runner)
add_test(
    NAME external_tests
    COMMAND ${CMAKE_COMMAND}
        --build ${CMAKE_BINARY_DIR}
        --target external_tests)

# Define an object library covering the source files in plugins/.
file(GLOB_RECURSE test_srcs_plugins CONFIGURE_DEPENDS
    "plugins/*.cpp")
add_library(test_lib_plugins OBJECT ${test_srcs_plugins})
target_link_libraries(test_lib_plugins PUBLIC
    cradle_external
    cradle_plugins_typing
    cradle_plugins_inner)

# Add the plugins test runner.
add_executable(plugins_test_runner)
target_link_libraries(plugins_test_runner PRIVATE
    catch2_runner
    basic_test_support
    test_lib_plugins)

# Add the plugins testing target.
add_custom_target(
    plugins_tests
    COMMAND $<TARGET_FILE:plugins_test_runner>
    DEPENDS plugins_test_runner)
add_test(
    NAME plugins_tests
    COMMAND ${CMAKE_COMMAND}
        --build ${CMAKE_BINARY_DIR}
        --target plugins_tests)

# Add a runner covering all unit tests (including external and inner).
add_executable(all_unit_tests_runner)
target_include_directories(all_unit_tests_runner PUBLIC
    ${generated_src_dir}
    src)
target_link_libraries(all_unit_tests_runner
    catch2_runner
    thinknode_test_support
    test_lib_unit
    test_lib_external
    test_lib_inner
    test_lib_plugins)
add_dependencies(all_unit_tests_runner
    copy_thinknode_dlls)

# Define an object library covering the source files in manual/.
file(GLOB_RECURSE test_srcs_manual CONFIGURE_DEPENDS
    "manual/*.cpp")
add_library(test_lib_manual OBJECT ${test_srcs_manual})
target_link_libraries(test_lib_manual PUBLIC
    cradle_inner
    cradle_plugins_inner)

# Add the manual test runner.
# The runner should be invoked directly, not via CMake.
add_executable(manual_test_runner)
target_include_directories(manual_test_runner PUBLIC
    ${generated_src_dir}
    ${int_gen_src_dir}
    src)
add_dependencies(manual_test_runner
    copy_cradle_dlls)
target_link_libraries(manual_test_runner PRIVATE
    catch2_runner
    basic_test_support
    cradle_rpclib_client
    test_lib_manual)

# Add a target running all unit tests (including external and inner).
add_custom_target(
    all_unit_tests
    # Create a fresh 'all-unit-testing' directory within the build dir and run the
    # tests within that. (Some of them perform file I/O.)
    COMMAND ${CMAKE_COMMAND} -E remove_directory all-unit-testing
    COMMAND ${CMAKE_COMMAND} -E make_directory all-unit-testing
    COMMAND ${CMAKE_COMMAND} -E chdir all-unit-testing ${CMAKE_COMMAND} -E env CRADLE_DEPLOY_DIR=${CMAKE_CURRENT_BINARY_DIR} CRADLE_THINKNODE_API_TOKEN=${API_TOKEN} CRADLE_THINKNODE_DOCKER_AUTH=${DOCKER_AUTH} $<TARGET_FILE:all_unit_tests_runner>
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    DEPENDS all_unit_tests_runner)
add_dependencies(all_unit_tests
    rpclib_server
    copy_cradle_dlls
    test_inner_dll_v1
    test_inner_dll_x0
    test_inner_dll_x1)
add_test(
    NAME all_unit_tests
    COMMAND ${CMAKE_COMMAND}
        --build ${CMAKE_BINARY_DIR}
        --target all_unit_tests)

# Add the unit test coverage target, covering all unit tests.
if(IS_CLANG AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    string(REGEX REPLACE "clang(\\+\\+)?" llvm-profdata LLVM_PROFDATA ${CMAKE_CXX_COMPILER})
    string(REGEX REPLACE "clang(\\+\\+)?" llvm-cov LLVM_COV ${CMAKE_CXX_COMPILER})
    # Set the shared libraries whose coverage is to be measured.
    # These should be the ones that all_unit_tests_runner is linked against, plus
    # those explicitly loaded by the unit tests, into the local test runner process.
    # Code executed in the rpclib server is not measured.
    set(coverage_objects
        cradle_inner cradle_plugins_inner cradle_rpclib_client
        cradle_typing cradle_plugins_typing cradle_thinknode cradle_websocket cradle_external)
    string(REGEX REPLACE "([a-z_]+)" "--object=$<TARGET_FILE:\\1>"
        coverage_object_options "${coverage_objects}")
    add_custom_target(
        unit_test_coverage
        COMMAND ${LLVM_PROFDATA} merge -sparse all-unit-testing/default.profraw
                -o default.profdata
        COMMAND ${LLVM_COV} show -instr-profile=default.profdata
                $<TARGET_FILE:all_unit_tests_runner>
                ${coverage_object_options}
                >${PROJECT_SOURCE_DIR}/coverage.txt
        COMMAND ${LLVM_COV} report -instr-profile=default.profdata
                $<TARGET_FILE:all_unit_tests_runner>
                ${coverage_object_options}
                >${PROJECT_SOURCE_DIR}/coverage-summary.txt
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
        DEPENDS all_unit_tests)
    add_test(
        NAME unit_test_coverage
        COMMAND ${CMAKE_COMMAND}
            --build ${CMAKE_BINARY_DIR}
            --target unit_test_coverage)
endif()

# Create an object library covering the source files in benchmarks/.
file(GLOB_RECURSE test_srcs_benchmark CONFIGURE_DEPENDS
    "benchmarks/*.cpp")
add_library(test_lib_benchmark OBJECT ${test_srcs_benchmark})
target_link_libraries(test_lib_benchmark PUBLIC
    cradle_external
    benchmark::benchmark)

# The benchmark library has its own test runner.
add_executable(benchmark_test_runner)
target_link_libraries(benchmark_test_runner PRIVATE
    thinknode_test_support
    test_lib_benchmark
    cradle_plugins_typing
    cradle_plugins_inner)
add_dependencies(benchmark_test_runner
    copy_thinknode_dlls)

add_custom_target(
    benchmark_tests
    COMMAND $<TARGET_FILE:benchmark_test_runner>
    DEPENDS benchmark_test_runner)
add_dependencies(benchmark_tests
    rpclib_server
    copy_cradle_dlls)
add_test(
    NAME benchmark_tests
    COMMAND ${CMAKE_COMMAND}
        --build ${CMAKE_BINARY_DIR}
        --target benchmark_tests)

if(IS_MSVC)
    # Copy CRADLE's DLLs to the directory containing the test executables.
    # Third-party DLLs are already there.
    # cppcoro is statically linked so has no DLL.
    add_custom_target(copy_cradle_dlls
        COMMAND ${CMAKE_COMMAND} -E echo "copy DLLs to $<TARGET_FILE_DIR:inner_test_runner>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:cradle_inner>
            $<TARGET_FILE:cradle_typing>
            $<TARGET_FILE:lib_third_party>
            $<TARGET_FILE:cradle_plugins_inner>
            $<TARGET_FILE:cradle_plugins_typing>
            $<TARGET_FILE:cradle_thinknode>
            $<TARGET_FILE:cradle_rpclib_client>
            $<TARGET_FILE:cradle_websocket>
            $<TARGET_FILE:cradle_external>
        $<TARGET_FILE_DIR:inner_test_runner>)
    add_dependencies(copy_cradle_dlls
        cradle_inner
        cradle_typing
        lib_third_party
        cradle_plugins_inner
        cradle_plugins_typing
        cradle_thinknode
        cradle_rpclib_client
        cradle_websocket
        cradle_external)
else()
    add_custom_target(copy_cradle_dlls)
endif()
