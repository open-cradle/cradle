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

# Define TSan (ThreadSanitizer) options.
option(CRADLE_THREAD_SANITIZER "Enable ThreadSanitizer (TSan)" OFF)

# Define UBSan (UndefinedBehaviorSanitizer) options.
option(CRADLE_UNDEFINED_BEHAVIOR_SANITIZER "Enable UndefinedBehaviorSanitizer (UBSan)" OFF)

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

# Some of CRADLE's libraries are intended to be dynamically loaded, so are
# built as MODULE libraries.
#
# Such a MODULE library is linked with other libraries; CRADLE and third-party
# ones (e.g. cppcoro). Preferably, all of these libraries will be SHARED
# ones. Having them STATIC would mean they will appear twice in the running
# executable: once from being linked into the executable, and once from being
# linked into the MODULE library. This not only is a waste of resources, but
# also causes multiple instances of singletons, leading to incorrect program
# behaviour.
#
# For this reason, all CRADLE libraries are SHARED. Third-party libraries
# currently seem to be STATIC, but this should change too. However, these
# STATIC libraries should be compiled into position-independent object code,
# otherwise they cannot be linked into the SHARED CRADLE libraries:
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# TODO third-party libraries (like cppcoro) should be SHARED ones.
#
# For libraries build via vcpkg, this is dictated by the VCPKG_TARGET_TRIPLET setting.
# For Linux, the value is x64-linux, and vcpkg/triplets/x64-linux.cmake has
#   set(VCPKG_TARGET_ARCHITECTURE x64)
#   set(VCPKG_CRT_LINKAGE dynamic)
#   set(VCPKG_LIBRARY_LINKAGE static)
# For Windows, the value is x64-windows, and vcpkg/triplets/x64-windows.cmake has
#   set(VCPKG_TARGET_ARCHITECTURE x64)
#   set(VCPKG_CRT_LINKAGE dynamic)
#   set(VCPKG_LIBRARY_LINKAGE dynamic)
# The difference is intentional, cf. https://github.com/microsoft/vcpkg/discussions/19127.
#
# cppcoro is exceptional in that it is not built via vcpkg.
# On Windows, the CRADLE buildtree contains cppcoro.lib but not cppcoro.dll.

# See CMake documentation for rationale.
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS 1)

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
    GIT_TAG 244f834274a850eeca4cc5c86acbca1a0fbe9d95)
FetchContent_MakeAvailable(fetched_cppcoro)

# Find core dependencies.
find_package(Boost REQUIRED COMPONENTS
    filesystem program_options random)
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
find_path(BSHOSHANY_THREAD_POOL_INCLUDE_DIRS "BS_thread_pool.hpp")

# The vcpkg tomlplusplus port now requires using pkg-config, which requires a
# separate install step on Windows, so to avoid that, just include it via
# FetchContent.
FetchContent_Declare(
    fetched_tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(fetched_tomlplusplus)

find_package(Git REQUIRED)
# The patch command may not be idempotent; if anything changes (like GIT_TAG),
# all build-directory/_deps/fetched_rpclib* should be removed before rebuilding.
FetchContent_Declare(fetched_rpclib
    GIT_REPOSITORY https://github.com/rpclib/rpclib.git
    GIT_TAG "v2.3.0"
    PATCH_COMMAND ${GIT_EXECUTABLE} apply ${CMAKE_CURRENT_SOURCE_DIR}/patches/rpclib.patch
    UPDATE_DISCONNECTED TRUE)
FetchContent_MakeAvailable(fetched_rpclib)

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
    # Workaround for https://github.com/andreasbuhr/cppcoro/issues/77:
    # set(CMAKE_CXX_STANDARD 20) causes CMake to append -std=gnu++2a,
    # causing a built-in -Dlinux=1,
    # causing build failures when "namespace linux" is encountered.
    add_compile_options(-Ulinux)
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
    # Warnings about a derived class inheriting member functions via dominance
    # (which is correct and intended).
    add_compile_options(/wd4250)
    # warnings about functions that are potentially insecure
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
    add_definitions(/D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
    # Also suppress linker warnings about missing .pdb files that seem to inevitably creep in.
    add_link_options(/ignore:4099)
    # "operator '|': deprecated between enumerations of different types"
    # in cereal\external\rapidjson\document.h
    add_compile_options(/wd5054)
    # The "unreachable code" warning is never to the point, and due to lack of
    # context impossible to fix.
    add_compile_options(/wd4702)
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

# Exclude as much of the Windows API as possible to avoid #define pollution
# and speed up builds. Note that this has to be done via command-line arguments
# due to the use of precompiled hearders.
if(IS_MSVC)
    add_definitions(/DWIN32_LEAN_AND_MEAN /DVC_EXTRALEAN /DNORASTEROPS /DOEMRESOURCE /DNOATOM /DNOCLIPBOARD /DNOCOLOR /DNOCTRLMGR /DNODRAWTEXT /DNOGDI /DNOKERNEL /DNOMINMAX /DNOSOUND /DNOKANJI /DNOHELP /DNOPROFILER /DNODEFERWINDOWPOS /DNOMCX)
endif()

if((IS_GCC OR IS_CLANG) AND CRADLE_GPROF_PROFILING)
    message(STATUS "Enabling CPU profiling using gprof")
    add_compile_options(-pg)
    # TODO also shared libs? add_link_options?
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -pg")
elseif(IS_CLANG AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Set build options for instrumenting test coverage.
    message(STATUS "Enabling gcov support")
    add_compile_options(-DLLVM_USE_LINKER=gold -fprofile-instr-generate
                        -fcoverage-mapping)
    # TODO replace with
    #   add_link_options(-fprofile-instr-generate -fcoverage-mapping)
    # and make it work.
    string(APPEND CMAKE_EXE_LINKER_FLAGS
           " -fprofile-instr-generate -fcoverage-mapping")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS
           " -fprofile-instr-generate -fcoverage-mapping")
endif()

if(CRADLE_THREAD_SANITIZER)
    message(STATUS "Enabling ThreadSanitizer")
    if(IS_GCC OR IS_CLANG)
        add_compile_options(-fsanitize=thread -g)
        # TODO also shared libs? add_link_options?
        string(APPEND CMAKE_EXE_LINKER_FLAGS " -fsanitize=thread")
    elseif(IS_MSVC)
       add_compile_options(/fsanitize=address /Zi)
    endif()
endif()

if(CRADLE_UNDEFINED_BEHAVIOR_SANITIZER)
    message(STATUS "Enabling UndefinedBehaviorSanitizer")
    if(IS_CLANG)
        # Make sure to use clang++ (not ld) as a linker; e.g.
        # CMAKE_LINKER:FILEPATH=/usr/bin/clang++-15
        add_compile_options(-fsanitize=undefined)
        # No need to pass this option to the linker, it seems
    else()
        message(FATAL_ERROR "UBSan supported for Clang only")
    endif()
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
        if("${CRADLE_COMMIT_HASH}" STREQUAL "")
            # Get the current commit hash for the CRADLE repository.
            execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
                OUTPUT_VARIABLE commit_hash
                OUTPUT_STRIP_TRAILING_WHITESPACE)
        else()
            set(commit_hash ${CRADLE_COMMIT_HASH})
        endif()
        # This fails if CMake is reinvoked, but given that it is only meant to
        # be used in automated builds of CRADLE as a dependency, it seems fine
        # to leave it as is until another use case emerges.
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

# Partition ${generated_srcs} based on src/cradle subdirectory.
set(generated_srcs_typing ${generated_srcs})
list(FILTER generated_srcs_typing INCLUDE REGEX "src/cradle/typing")
set(generated_srcs_thinknode ${generated_srcs})
list(FILTER generated_srcs_thinknode INCLUDE REGEX "src/cradle/thinknode")
set(generated_srcs_websocket ${generated_srcs})
list(FILTER generated_srcs_websocket INCLUDE REGEX "src/cradle/websocket")

# Files generated by the preprocessor are in ${generated_src_dir};
# depending on the ${CRADLE_PREPROCESSOR_USAGE} setting, generated by this cmake,
# or provided through other means.
# Other generated files are in ${intgen_src_dir}; always generated by this cmake.
set(intgen_src_dir
    ${CMAKE_CURRENT_BINARY_DIR}/internally_generated/src)

# Establish the location of deploy_dir.h (which will be created later on).
set(deploy_dir_file "${intgen_src_dir}/cradle/deploy_dir.h")

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
add_custom_target(deploy_dir_file_target DEPENDS ${deploy_dir_file})

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
add_library(lib_third_party SHARED ${srcs_third_party})
record_public_include_directory(lib_third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third-party
    include/third-party
    "*.[ih]*")

# The library for the inner core
file(GLOB_RECURSE srcs_inner CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/inner/*.cpp")
add_library(cradle_inner SHARED ${srcs_inner})
add_dependencies(cradle_inner version_info)
# simdjson.h "local variable is initialized but not referenced"
# Only when building from Visual Studio GUI, not from command line?!
# TODO use recent simdjson
if(IS_MSVC)
    set_property(
        SOURCE src/cradle/inner/service/config_map_from_json.cpp
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
record_public_include_directory(cradle_inner
    ${BSHOSHANY_THREAD_POOL_INCLUDE_DIRS}
    include/third-party
    "*.[ih]*")
target_link_libraries(cradle_inner PUBLIC
    Boost::filesystem
    Boost::headers
    cereal::cereal
    cppcoro
    CURL::libcurl
    fmt::fmt
    lz4::lz4
    msgpackc-cxx
    OpenSSL::SSL
    nlohmann_json::nlohmann_json
    simdjson::simdjson
    spdlog::spdlog
    tomlplusplus::tomlplusplus)

# A library for the plugins depending on the inner library
file(GLOB_RECURSE srcs_plugins_inner CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/plugins/secondary_cache/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/plugins/requests_storage/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/plugins/domain/testing/*.cpp")
add_library(cradle_plugins_inner SHARED ${srcs_plugins_inner})
record_public_include_directory(cradle_plugins_inner
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    include
    "*.h")
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
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/rpclib/common/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/rpclib/client/*.cpp")
add_library(cradle_rpclib_client SHARED)
target_sources(cradle_rpclib_client PRIVATE
    ${srcs_rpclib_client})
add_dependencies(cradle_rpclib_client deploy_dir_file_target)
record_public_include_directory(cradle_rpclib_client
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    include
    "*.h")
target_link_libraries(cradle_rpclib_client PUBLIC
    cradle_inner
    Boost::headers
    cppcoro
    rpc
    spdlog::spdlog)

# The typing library (a.o.) adds a type system (e.g. dynamics)
file(GLOB_RECURSE srcs_typing CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/typing/*.cpp")
add_library(cradle_typing SHARED ${srcs_typing} ${headers})
target_sources(cradle_typing PRIVATE
    ${generated_headers}
    ${generated_srcs_typing})
record_public_include_directory(cradle_typing
    ${generated_src_dir}
    include/generated
    "*.hpp")
record_public_include_directory(cradle_typing
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    include
    "*.h")
target_link_libraries(cradle_typing PUBLIC
    cradle_inner
    Boost::headers
    Catch2::Catch2
    cppcoro
    spdlog::spdlog
    yaml-cpp)

# The thinknode library adds some Thinknode requests
file(GLOB_RECURSE srcs_thinknode CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/thinknode/*.cpp")
add_library(cradle_thinknode SHARED ${srcs_thinknode} ${headers})
target_sources(cradle_thinknode PRIVATE
    ${generated_headers}
    ${generated_srcs_typing}
    ${generated_srcs_thinknode})
target_link_libraries(cradle_thinknode PUBLIC
    cradle_typing
    Boost::headers
    cereal::cereal
    cppcoro
    spdlog::spdlog)

# The websocket library adds more Thinknode requests, and a WebSocket interface
file(GLOB_RECURSE srcs_websocket CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/websocket/*.cpp")
add_library(cradle_websocket SHARED ${srcs_websocket} ${headers})
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
add_dependencies(cradle_websocket
    thinknode_dlls_dir_target)
# Boost::random required by websocketpp even though not advertised
target_link_libraries(cradle_websocket PUBLIC
    cradle_thinknode
    cradle_rpclib_client
    cradle_plugins_inner
    Boost::headers
    Boost::random
    cereal::cereal
    cppcoro
    spdlog::spdlog
    websocketpp::websocketpp)

# The external library adds a C++ API
file(GLOB_RECURSE srcs_external CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/external/*.cpp")
add_library(cradle_external SHARED ${srcs_external} ${headers})
target_sources(cradle_external PRIVATE ${generated_headers} ${generated_srcs})
target_link_libraries(cradle_external PUBLIC
    cradle_websocket)

# The WebSocket server
add_executable(websocket_server src/server.cpp)
target_link_libraries(websocket_server
    cradle_websocket
    Boost::program_options)

# The rpclib server
file(GLOB_RECURSE srcs_rpclib_server CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/rpclib/common/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cradle/rpclib/server/*.cpp")
add_executable(rpclib_server ${srcs_rpclib_server})
target_link_libraries(rpclib_server PRIVATE
    cradle_thinknode
    cradle_plugins_inner
    Boost::headers
    Boost::program_options
    cppcoro
    fmt::fmt
    rpc
    spdlog::spdlog)

# Convenience target denoting all servers
add_custom_target(servers)
add_dependencies(servers websocket_server rpclib_server)

# Create dynamic-loadable DLLs
add_subdirectory(src/cradle-dll)

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
            cradle_inner cradle_typing lib_third_party cradle_plugins_inner
            cradle_thinknode
            cradle_rpclib_client cradle_websocket cradle_external cppcoro
            tomlplusplus_tomlplusplus rpc
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
