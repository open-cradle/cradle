cmake_minimum_required(VERSION 3.14)

# CRADLE_IS_MAIN_PROJECT defines whether we are building CRADLE as the main
# project or as a dependency of another project.
if(NOT DEFINED CRADLE_IS_MAIN_PROJECT)
    if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        set(CRADLE_IS_MAIN_PROJECT ON)
    else()
        set(CRADLE_IS_MAIN_PROJECT OFF)
    endif()
endif()

# Define an option for generating installation targets.
option(CRADLE_INSTALL "Generate install targets" OFF)

# When building as part of the installation of a released version, the version
# number must be externally specified.
set(CRADLE_VERSION "" CACHE STRING
    "the CRADLE version number - Only for use in automated release builds.")
if(CRADLE_INSTALL AND NOT CRADLE_VERSION)
    message(FATAL "When installing, CRADLE_VERSION is required.")
endif()

# Again, during installation, the commit hash should be externally specified.
set(CRADLE_COMMIT_HASH "" CACHE STRING
    "the Git commit hash for this version of CRADLE")
if(CRADLE_VERSION AND NOT CRADLE_COMMIT_HASH)
    message(FATAL "When CRADLE_VERSION is specified, CRADLE_COMMIT_HASH is required.")
endif()

if(CRADLE_VERSION)
    project(cradle VERSION ${CRADLE_VERSION})
else()
    project(cradle)
endif()

# Define the options for using the preprocessor.
set(CRADLE_PREPROCESSOR_USAGE_OPTIONS Native GitCache External)
set(CRADLE_PREPROCESSOR_USAGE Native CACHE STRING
    "How the preprocessor should be invoked")
set_property(CACHE CRADLE_PREPROCESSOR_USAGE PROPERTY
    STRINGS ${CRADLE_PREPROCESSOR_USAGE_OPTIONS})
set(CRADLE_EXTERNALLY_PREPROCESSED_DIRECTORY "" CACHE STRING
    "If CRADLE_PREPROCESSOR_USAGE is External, this is the directory that contains the externally preprocessed files.")
# Note: If CRADLE_PREPROCESSOR_USAGE is GitCache, CRADLE_COMMIT_HASH is the
# commit hash to look up. If not explicitly given, it will be queried from Git.

# Define test generation options.
option(
    CRADLE_ENABLE_TESTING
    "Generate targets for testing CRADLE"
    ${CRADLE_IS_MAIN_PROJECT})
if(CRADLE_ENABLE_TESTING)
    enable_testing()
endif()
option(
    CRADLE_LOCAL_DOCKER_TESTING
    "Enable testing that requires a local Docker instance"
    ${CRADLE_ENABLE_TESTING})
if(CRADLE_LOCAL_DOCKER_TESTING)
    add_compile_options(-DCRADLE_LOCAL_DOCKER_TESTING)
endif()

# Define profiling options.
option(CRADLE_GPROF_PROFILING "Enable CPU profiling using gprof" OFF)

# Detect the compiler.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(IS_CLANG true)
else()
    set(IS_CLANG false)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(IS_GCC true)
else()
    set(IS_GCC false)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(IS_MSVC true)
else()
    set(IS_MSVC false)
endif()

# CRADLE requires C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# version_info.h defines the CRADLE version.
if(CRADLE_VERSION)
    set(CRADLE_SOURCE_IS_DIRTY "false")
    set(CRADLE_COMMITS_SINCE_RELEASE 0)
    # If a version was set externally, use it directly.
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/version_info.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/internally_generated/src/cradle/version_info.h"
        @ONLY)
    # Add a target to represent the version info.
    add_library(version_info INTERFACE)
else()
    # Otherwise, use version.cmake to extract the information from
    # `git describe` and generate the version_info.h from that.
    # We use a custom target here so that the command will always run.
    add_custom_target(version_info
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cmake)
endif()

include(FetchContent)

# The cppcoro detection logic seems broken for GCC 10.x (at least in CI), so
# override it.
if(IS_GCC OR IS_CLANG)
    add_library(std::coroutines INTERFACE IMPORTED)
    set(CXX_COROUTINES_HAVE_COROUTINES TRUE CACHE BOOL "TRUE if we have the C++ coroutines feature" FORCE)
    set(CXX_COROUTINES_HEADER coroutine CACHE STRING "The header that should be included to obtain the coroutines APIs" FORCE)
    set(CXX_COROUTINES_NAMESPACE std CACHE STRING "The C++ namespace that contains the coroutines APIs" FORCE)
    set(Coroutines_FOUND TRUE CACHE BOOL "TRUE if we can compile and link a program using std::coroutines" FORCE)
    if(IS_GCC)
        add_compile_options(-fcoroutines)
    endif()
endif()

FetchContent_Declare(fetched_cppcoro
    GIT_REPOSITORY https://github.com/andreasbuhr/cppcoro
    GIT_TAG ba05f5c4cfeff9fde1f80c8b354c4506cb446024)
FetchContent_MakeAvailable(fetched_cppcoro)

# Find core dependencies.
find_package(Boost REQUIRED COMPONENTS
    filesystem program_options)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(msgpack CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(websocketpp CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(simdjson CONFIG REQUIRED)
find_package(lz4 CONFIG REQUIRED)
find_package(cereal CONFIG REQUIRED)
find_package(rpclib CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)

# TODO: Isolate these so they're only required for testing.
find_package(Catch2 CONFIG REQUIRED)
find_package(benchmark CONFIG REQUIRED)

# Add the given linker options on anything that gets linked.
macro(add_link_options )
    string(REPLACE ";" " " OPTIONS "${ARGV}")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${OPTIONS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OPTIONS}")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} ${OPTIONS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${OPTIONS}")
endmacro()

# Add the given linker options for executables.
macro(add_exe_link_options )
    string(REPLACE ";" " " OPTIONS "${ARGV}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OPTIONS}")
endmacro()

# Enable a high level of compiler warnings and treat them as errors.
if(IS_GCC OR IS_CLANG)
    add_compile_options(-Wall -Werror -Wextra -pedantic)
    # Disable warnings that are too strict.
    # unused function parameters
    add_compile_options(-Wno-unused-parameter)
elseif(IS_MSVC)
    # First strip out the old warning level.
    string(REPLACE "/W3" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    add_compile_options(/W4 /WX)
    # Disable warnings that are too strict.
    # "unreferenced formal parameter"
    add_compile_options(/wd4100)
    # "declaration hides previous local declaration"
    add_compile_options(/wd4456)
    # "unreferenced local function has been removed"
    add_compile_options(/wd4505)
    # warnings about functions that are potentially insecure
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
    add_definitions(/D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
    # Also suppress linker warnings about missing .pdb files that seem to inevitably creep in.
    add_link_options(/ignore:4099)
    # "operator '|': deprecated between enumerations of different types"
    # in cereal\external\rapidjson\document.h
    add_compile_options(/wd5054)
endif()

# Use absolute paths for error messages.
if (IS_CLANG)
    add_compile_options(-fdiagnostics-absolute-paths)
elseif(IS_MSVC)
    add_compile_options(/FC)
endif()
# Full backtrace in error messages.
if(IS_GCC OR IS_CLANG)
    add_compile_options(-ftemplate-backtrace-limit=0)
endif()

# Enable big objects on MSVC.
if(IS_MSVC)
    add_compile_options(/bigobj)
endif()

# Enable the address sanitizer (for now).
# if(IS_MSVC)
#     add_compile_options(/fsanitize=address /Zi)
# endif()

# Exclude as much of the Windows API as possible to avoid #define pollution
# and speed up builds. Note that this has to be done via command-line arguments
# due to the use of precompiled hearders.
if(IS_MSVC)
    add_definitions(/DWIN32_LEAN_AND_MEAN /DVC_EXTRALEAN /DNORASTEROPS /DOEMRESOURCE /DNOATOM /DNOCLIPBOARD /DNOCOLOR /DNOCTRLMGR /DNODRAWTEXT /DNOGDI /DNOKERNEL /DNOMINMAX /DNOSOUND /DNOKANJI /DNOHELP /DNOPROFILER /DNODEFERWINDOWPOS /DNOMCX)
endif()

if((IS_GCC OR IS_CLANG) AND CRADLE_GPROF_PROFILING)
    message(STATUS "Enabling CPU profiling using gprof")
    add_compile_options(-pg)
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -pg")
elseif(IS_CLANG AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Set build options for instrumenting test coverage.
    message(STATUS "Enabling gcov support")
    add_compile_options(-DLLVM_USE_LINKER=gold -fprofile-instr-generate
                        -fcoverage-mapping)
    string(APPEND CMAKE_EXE_LINKER_FLAGS
           " -fprofile-instr-generate -fcoverage-mapping")
endif()

# Preprocess the API headers.
file(GLOB_RECURSE headers CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/*.hpp")
if("${CRADLE_PREPROCESSOR_USAGE}" STREQUAL "Native")
    message(STATUS "Using native preprocessor")
    add_subdirectory(preprocessor)
    set(preprocessed_files ${headers})
    preprocess_header_files(
        generated_srcs generated_headers
        ACCOUNT_ID mgh
        # TODO: Make this its own actual app...
        TYPE_APP_ID dosimetry
        FUNCTION_APP_ID dosimetry
        NAMESPACE cradle
        INDEX_FILE "${CMAKE_CURRENT_BINARY_DIR}/generated/src/cradle/api_index.hpp"
        INPUT_FILES ${preprocessed_files})
    add_custom_target(preprocessing
        DEPENDS ${generated_srcs} ${generated_headers})
    set(generated_src_dir
        ${CMAKE_CURRENT_BINARY_DIR}/generated/src)
elseif("${CRADLE_PREPROCESSOR_USAGE}" STREQUAL "GitCache" OR
       "${CRADLE_PREPROCESSOR_USAGE}" STREQUAL "External")
    if("${CRADLE_PREPROCESSOR_USAGE}" STREQUAL "GitCache")
        message(STATUS "Retrieving preprocessed files from Git cache")
        find_package(Git)
            if("${CRADLE_COMMIT_HASH}" STREQUAL "")
                # Get the current commit hash for the CRADLE repository.
                execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
                    OUTPUT_VARIABLE commit_hash)
                # Strip trailing newline.
                string(REPLACE "\n" "" commit_hash "${commit_hash}")
            else()
                set(commit_hash ${CRADLE_COMMIT_HASH})
            endif()
            # This fails if CMake is reinvoked, but given that it is only meant
            # to be used in automated builds of CRADLE as a dependency, it
            # seems fine to leave it as is until another use case emerges.
            execute_process(
                COMMAND ${GIT_EXECUTABLE} clone https://github.com/open-cradle/preprocessor-output
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        execute_process(
            COMMAND ${GIT_EXECUTABLE} checkout "output-${commit_hash}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/preprocessor-output")
        set(CRADLE_EXTERNALLY_PREPROCESSED_DIRECTORY
            "${CMAKE_CURRENT_BINARY_DIR}/preprocessor-output/generated/")
    endif()
    message(STATUS "Using externally preprocessed files in:")
    message(STATUS "  ${CRADLE_EXTERNALLY_PREPROCESSED_DIRECTORY}")
    get_filename_component(
        resolved_preprocessed_directory
        "${CRADLE_EXTERNALLY_PREPROCESSED_DIRECTORY}"
        ABSOLUTE)
    message(STATUS "  (${resolved_preprocessed_directory})")
    file(GLOB_RECURSE generated_srcs
        "${resolved_preprocessed_directory}/*.cpp")
    file(GLOB_RECURSE generated_headers
        "${resolved_preprocessed_directory}/*.hpp")
    add_custom_target(preprocessing)
    set(generated_src_dir
        ${resolved_preprocessed_directory}/src)
else()
    message(FATAL_ERROR "Invalid setting for CRADLE_PREPROCESSOR_USAGE")
endif()

# Files generated by the preprocessor are in ${generated_src_dir};
# depending on the ${CRADLE_PREPROCESSOR_USAGE} setting, generated by this cmake,
# or provided through other means.
# Other generated files are in ${intgen_src_dir}; always generated by this cmake.
set(intgen_src_dir
    ${CMAKE_CURRENT_BINARY_DIR}/internally_generated/src)

# Establish the location of deploy_dir.h (which will be created later on).
set(deploy_dir_file "${intgen_src_dir}/cradle/deploy_dir.h")

# Set up a public include directory for a target in a way that's compatible
# with installing the headers in those directories for use by clients.
# Note that using file sets may solve this problem more elegantly, but they are
# only supported by more recent versions of CMake.
function(record_public_include_directory
    TARGET INTERNAL_DIRECTORY INSTALLED_DIRECTORY PATTERN)
    target_include_directories(${TARGET} PUBLIC
        $<BUILD_INTERFACE:${INTERNAL_DIRECTORY}>
        $<INSTALL_INTERFACE:${INSTALLED_DIRECTORY}>)
    if(CRADLE_INSTALL)
        install(
            DIRECTORY ${INTERNAL_DIRECTORY}/
            DESTINATION ${INSTALLED_DIRECTORY}
            FILES_MATCHING
            PATTERN ${PATTERN})
    endif()
endfunction()

# The library for the in-tree third-party software
file(GLOB_RECURSE srcs_third_party CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/third-party/*.cpp")
add_library(lib_third_party STATIC ${srcs_third_party})
record_public_include_directory(lib_third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third-party
    include/third-party
    "*.[ih]*")

# The library for the inner core
file(GLOB_RECURSE srcs_inner CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/inner/*.cpp")
add_library(cradle_inner STATIC ${srcs_inner})
add_dependencies(cradle_inner version_info)
# simdjson.h "local variable is initialized but not referenced"
# Only when building from Visual Studio GUI, not from command line?!
# TODO use recent simdjson
if(IS_MSVC)
    set_property(
        SOURCE src/cradle/inner/service/config_map_json.cpp
        APPEND_STRING PROPERTY COMPILE_FLAGS " /wd4189")
endif()
record_public_include_directory(cradle_inner
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    include
    "*.h")
record_public_include_directory(cradle_inner
    ${intgen_src_dir}
    include/internally_generated
    "*.h*")
record_public_include_directory(cradle_inner
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third-party
    include/third-party
    "*.[ih]*")
target_link_libraries(cradle_inner PUBLIC
    Boost::headers
    cereal::cereal
    cppcoro
    CURL::libcurl
    fmt::fmt
    lz4::lz4
    msgpackc-cxx
    OpenSSL::SSL
    simdjson::simdjson
    spdlog::spdlog
    tomlplusplus::tomlplusplus)

# A library for the plugins depending on the inner library
file(GLOB_RECURSE srcs_plugins_inner CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/plugins/secondary_cache/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/plugins/domain/testing/*.cpp")
add_library(cradle_plugins_inner STATIC ${srcs_plugins_inner})
add_dependencies(cradle_plugins_inner preprocessing)
record_public_include_directory(cradle_plugins_inner
    ${generated_src_dir}
    include/generated
    "*.hpp")
record_public_include_directory(cradle_plugins_inner
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    include
    "*.h*")
target_link_libraries(cradle_plugins_inner PUBLIC
    cradle_inner
    lib_third_party
    Boost::headers
    cereal::cereal
    cppcoro
    spdlog::spdlog
    SQLite::SQLite3)

# The library for the rpclib client
file(GLOB_RECURSE srcs_rpclib_client CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/rpclib/client/*.cpp")
add_library(cradle_rpclib_client STATIC)
target_sources(cradle_rpclib_client PRIVATE
    ${srcs_rpclib_client}
    ${deploy_dir_file})
record_public_include_directory(cradle_rpclib_client
    ${generated_src_dir}
    include/generated
    "*.hpp")
record_public_include_directory(cradle_rpclib_client
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    include
    "*.h*")
target_link_libraries(cradle_rpclib_client PUBLIC
    cradle_inner
    Boost::filesystem
    Boost::headers
    cppcoro
    rpclib::rpc
    spdlog::spdlog)

# The typing library (a.o.) adds a type system (e.g. dynamics)
file(GLOB_RECURSE srcs_typing CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/typing/*.cpp")
add_library(cradle_typing STATIC ${srcs_typing} ${headers})
target_sources(cradle_typing PRIVATE ${generated_headers} ${generated_srcs})
record_public_include_directory(cradle_typing
    ${generated_src_dir}
    include/generated
    "*.hpp")
record_public_include_directory(cradle_typing
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    include
    "*.h*")
target_link_libraries(cradle_typing PUBLIC
    cradle_inner
    Boost::headers
    Catch2::Catch2
    cppcoro
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    yaml-cpp)

# The library for the plugins depending on the typing library
file(GLOB_RECURSE srcs_plugins_typing CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/plugins/serialization/secondary_cache/legacy/native/*.cpp")
add_library(cradle_plugins_typing STATIC ${srcs_plugins_typing})
add_dependencies(cradle_plugins_typing preprocessing)
target_link_libraries(cradle_plugins_typing PUBLIC
    cradle_typing
    cppcoro)

# The thinknode library adds some Thinknode requests
file(GLOB_RECURSE srcs_thinknode CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/thinknode/*.cpp")
add_library(cradle_thinknode STATIC ${srcs_thinknode} ${headers})
target_sources(cradle_thinknode PRIVATE ${generated_headers} ${generated_srcs})
target_link_libraries(cradle_thinknode PUBLIC
    cradle_plugins_typing
    cradle_typing
    Boost::headers
    cereal::cereal
    cppcoro
    spdlog::spdlog)

# A library of plugins depending on the thinknode library
file(GLOB_RECURSE srcs_plugins_thinknode CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/plugins/domain/all/*.cpp")
add_library(cradle_plugins_thinknode STATIC ${srcs_plugins_thinknode})
target_link_libraries(cradle_plugins_thinknode PUBLIC
    cradle_thinknode)

# The websocket library adds more Thinknode requests, and a WebSocket interface
file(GLOB_RECURSE srcs_websocket CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/websocket/*.cpp")
add_library(cradle_websocket STATIC ${srcs_websocket} ${headers})
# There doesn't seem to be a better way to disable the warnings that are
# generated by WebSocket++.
if(IS_MSVC)
    set_property(
        SOURCE src/cradle/websocket/server.cpp
        APPEND_STRING PROPERTY COMPILE_FLAGS " /wd4996 /wd4127 /wd4267 /wd4244")
    set_property(
        SOURCE src/cradle/websocket/client.cpp
        APPEND_STRING PROPERTY COMPILE_FLAGS " /wd4996 /wd4127 /wd4267 /wd4244")
endif()
target_sources(cradle_websocket PRIVATE ${generated_headers} ${generated_srcs})
target_link_libraries(cradle_websocket PUBLIC
    cradle_thinknode
    cradle_rpclib_client
    Boost::headers
    cereal::cereal
    cppcoro
    spdlog::spdlog
    websocketpp::websocketpp)

# The external library adds a C++ API
file(GLOB_RECURSE srcs_external CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/external/*.cpp")
add_library(cradle_external STATIC ${srcs_external} ${headers})
target_sources(cradle_external PRIVATE ${generated_headers} ${generated_srcs})
target_link_libraries(cradle_external PUBLIC
    cradle_websocket)

# The WebSocket server
add_executable(websocket_server src/server.cpp)
target_link_libraries(websocket_server
    cradle_websocket
    cradle_plugins_inner
    Boost::program_options)
# add_custom_command(TARGET websocket_server POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:websocket_server> ${deploy_dir})

# The rpclib server
file(GLOB_RECURSE srcs_rpclib_server CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/rpclib/server/*.cpp")
add_executable(rpclib_server ${srcs_rpclib_server})
target_link_libraries(rpclib_server PRIVATE
    cradle_plugins_thinknode
    cradle_thinknode
    cradle_plugins_inner
    Boost::headers
    Boost::program_options
    cppcoro
    fmt::fmt
    rpclib::rpc
    spdlog::spdlog)
# add_custom_command(TARGET rpclib_server POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:rpclib_server> ${deploy_dir})

# Create deploy_dir.h for providing access to the rpclib server executable.
add_custom_command(
    OUTPUT ${deploy_dir_file}
    COMMAND ${CMAKE_COMMAND}
        -D input_dir=${CMAKE_CURRENT_SOURCE_DIR}/cmake
        -D output_dir=${intgen_src_dir}/cradle
        -D deployed_executable="$<TARGET_FILE:rpclib_server>"
        -D compiler_id=${CMAKE_CXX_COMPILER_ID}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/deploy_dir.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/deploy_dir.h.in)

# Convenience target denoting all servers
add_custom_target(servers)
add_dependencies(servers websocket_server rpclib_server)

if (CRADLE_ENABLE_TESTING)
    # Retrieve the token info if it exists
    if (DEFINED ENV{CRADLE_THINKNODE_API_TOKEN})
        set(API_TOKEN $ENV{CRADLE_THINKNODE_API_TOKEN})
    elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.token")
        file(READ "${CMAKE_CURRENT_SOURCE_DIR}/.token" API_TOKEN)
    else()
        message(WARNING "Couldn't find API token.")
        set(API_TOKEN "(missing)")
    endif()
    string(REGEX REPLACE "\n" "" API_TOKEN "${API_TOKEN}")

    # Retrieve the Docker auth info if it exists
    if (DEFINED ENV{CRADLE_THINKNODE_DOCKER_AUTH})
        set(DOCKER_AUTH $ENV{CRADLE_THINKNODE_DOCKER_AUTH})
    elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.docker-auth")
        file(READ "${CMAKE_CURRENT_SOURCE_DIR}/.docker-auth" DOCKER_AUTH)
    else()
        message(WARNING "Couldn't find Docker auth string.")
        set(DOCKER_AUTH "(missing)")
    endif()
    string(REGEX REPLACE "\n" "" DOCKER_AUTH "${DOCKER_AUTH}")

    add_subdirectory(tests)
    add_subdirectory(python)
endif()

# Describe our installation.
if(CRADLE_INSTALL)
    install(
        TARGETS
            cradle_inner cradle_typing cradle_plugins_inner
            cradle_plugins_typing cradle_thinknode cradle_plugins_thinknode
            cradle_rpclib_client cradle_websocket cradle_external cppcoro
        EXPORT CradleTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin)

    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        CradleConfigVersion.cmake
        VERSION ${CRADLE_VERSION}
        COMPATIBILITY AnyNewerVersion)

    install(
        EXPORT CradleTargets
        FILE CradleTargets.cmake
        NAMESPACE cradle::
        DESTINATION lib/cmake/cradle)

    configure_file(cmake/CradleConfig.cmake.in CradleConfig.cmake @ONLY)
    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/CradleConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/CradleConfigVersion.cmake"
        DESTINATION lib/cmake/cradle)

    FetchContent_GetProperties(
        fetched_cppcoro
        SOURCE_DIR FETCHED_CPPCORO_SOURCE_DIR)
    install(
        FILES
            "${FETCHED_CPPCORO_SOURCE_DIR}/cmake/FindCoroutines.cmake"
        DESTINATION lib/cmake/cradle)

    # Describe our packaging.
    set(CPACK_PACKAGE_VENDOR "CRADLE")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "declarative calculation framework")
    set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    include(CPack)
endif()
