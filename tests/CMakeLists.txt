# Define an object library for the test runner.
# (An object library is a CMake artifact. The linker will see the object files, not a library.)
add_library(test_lib_runner OBJECT
    runner.cpp)
target_link_libraries(test_lib_runner PUBLIC
    cradle_inner
    Catch2::Catch2
    spdlog::spdlog)

# Don't use precompiled headers for the test runner since it needs to include
# Catch specially.
set_source_files_properties(
    runner.cpp
    PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON)

# Define an object library covering the source files in unit/.
file(GLOB_RECURSE test_srcs_unit CONFIGURE_DEPENDS
    "unit/*.cpp")
add_library(test_lib_unit OBJECT ${test_srcs_unit})
target_link_libraries(test_lib_unit PUBLIC
    cradle_websocket
    cradle_plugins_typing
    cradle_plugins_inner
    fakeit::fakeit)

# Add the unit test runner.
add_executable(unit_test_runner)
target_sources(unit_test_runner PRIVATE
    support/inner_service.cpp
    support/outer_service.cpp
    support/thinknode.cpp)
target_include_directories(unit_test_runner PUBLIC
    ${generated_src_dirs}
    src)
target_link_libraries(unit_test_runner PRIVATE
    test_lib_runner
    test_lib_unit
    cradle_inner)

# Add the unit testing target.
add_custom_target(
    unit_tests
    # Create a fresh 'unit-testing' directory within the build dir and run the
    # tests within that. (Some of them perform file I/O.)
    COMMAND ${CMAKE_COMMAND} -E remove_directory unit-testing
    COMMAND ${CMAKE_COMMAND} -E make_directory unit-testing
    COMMAND ${CMAKE_COMMAND} -E chdir unit-testing ${CMAKE_COMMAND} -E env CRADLE_DEPLOY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy CRADLE_THINKNODE_API_TOKEN=${API_TOKEN} CRADLE_THINKNODE_DOCKER_AUTH=${DOCKER_AUTH} $<TARGET_FILE:unit_test_runner>
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    DEPENDS unit_test_runner)
add_test(
    NAME unit_tests
    COMMAND ${CMAKE_COMMAND}
        --build ${CMAKE_BINARY_DIR}
        --target unit_tests)

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

# Add the inner test runner.
add_executable(inner_test_runner)
target_sources(inner_test_runner PRIVATE
    support/inner_service.cpp)
target_link_libraries(inner_test_runner PRIVATE
    test_lib_runner
    test_lib_inner)

# Add the inner testing target.
add_custom_target(
    inner_tests
    COMMAND $<TARGET_FILE:inner_test_runner>
    DEPENDS inner_test_runner)
add_dependencies(inner_tests rpclib_server)
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
target_sources(external_test_runner PRIVATE
    support/outer_service.cpp)
target_include_directories(external_test_runner PUBLIC
    ${generated_src_dirs}
    src)
target_link_libraries(external_test_runner PRIVATE
    test_lib_external
    test_lib_runner)

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
target_sources(plugins_test_runner PRIVATE
    support/inner_service.cpp)
target_link_libraries(plugins_test_runner PRIVATE
    test_lib_runner
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
target_sources(all_unit_tests_runner PRIVATE
    support/inner_service.cpp
    support/outer_service.cpp
    support/thinknode.cpp)
target_include_directories(all_unit_tests_runner PUBLIC
    ${generated_src_dirs}
    src)
target_link_libraries(all_unit_tests_runner
    test_lib_runner
    test_lib_unit
    test_lib_external
    test_lib_inner
    test_lib_plugins)

# Add a target running all unit tests (including external and inner).
add_custom_target(
    all_unit_tests
    # Create a fresh 'all-unit-testing' directory within the build dir and run the
    # tests within that. (Some of them perform file I/O.)
    COMMAND ${CMAKE_COMMAND} -E remove_directory all-unit-testing
    COMMAND ${CMAKE_COMMAND} -E make_directory all-unit-testing
    COMMAND ${CMAKE_COMMAND} -E chdir all-unit-testing ${CMAKE_COMMAND} -E env CRADLE_DEPLOY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy CRADLE_THINKNODE_API_TOKEN=${API_TOKEN} CRADLE_THINKNODE_DOCKER_AUTH=${DOCKER_AUTH} $<TARGET_FILE:all_unit_tests_runner>
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    DEPENDS all_unit_tests_runner)
add_dependencies(all_unit_tests rpclib_server)
add_test(
    NAME all_unit_tests
    COMMAND ${CMAKE_COMMAND}
        --build ${CMAKE_BINARY_DIR}
        --target all_unit_tests)

# Add the unit test coverage target, covering all unit tests.
if(IS_CLANG AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    string(REGEX REPLACE "clang(\\+\\+)?" llvm-profdata LLVM_PROFDATA ${CMAKE_CXX_COMPILER})
    string(REGEX REPLACE "clang(\\+\\+)?" llvm-cov LLVM_COV ${CMAKE_CXX_COMPILER})
    add_custom_target(
        unit_test_coverage
        COMMAND ${LLVM_PROFDATA} merge -sparse all-unit-testing/default.profraw
                -o default.profdata
        COMMAND ${LLVM_COV} show -instr-profile=default.profdata
                $<TARGET_FILE:all_unit_tests_runner>
                >${PROJECT_SOURCE_DIR}/coverage.txt
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

add_executable(benchmark_test_runner)
target_sources(benchmark_test_runner PRIVATE
    support/inner_service.cpp 
    support/outer_service.cpp)
target_link_libraries(benchmark_test_runner PRIVATE
    test_lib_benchmark
    cradle_plugins_typing
    cradle_plugins_inner)

add_custom_target(
    benchmark_tests
    COMMAND $<TARGET_FILE:benchmark_test_runner>
    DEPENDS benchmark_test_runner)
add_dependencies(benchmark_tests rpclib_server)
add_test(
    NAME benchmark_tests
    COMMAND ${CMAKE_COMMAND}
        --build ${CMAKE_BINARY_DIR}
        --target benchmark_tests)
