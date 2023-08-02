# Run 'git describe' and capture its output.
find_package(Git REQUIRED)
execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --long
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE git_description
                RESULT_VARIABLE result)
if(result AND NOT result EQUAL 0)
    message(FATAL_ERROR "'git describe' failed: ${result}")
endif()
# Strip trailing newline.
string(REPLACE "\n" "" git_description "${git_description}")
# Split the output into its fragments.
# The description is in the form "<tag>-<commits-since-tag>-<hash>[-dirty]".
# (Note that <tag> may have internal '-' characters'.)
string(REPLACE "-" ";" fragments "${git_description}")
# Even if the tag doesn't have any internal dashes and there is no dirty
# component, we should still have three fragments in the description.
list(LENGTH fragments fragment_count)
if(${fragment_count} LESS 3)
    message(FATAL_ERROR
        "unable to parse 'git describe' output:\n${git_description}")
endif()

# Now work backwards interpreting the parts...

# Check for the dirty flag.
set(CRADLE_SOURCE_IS_DIRTY FALSE)
math(EXPR index "${fragment_count} - 1")
list(GET fragments ${index} tail)
if (tail STREQUAL "dirty")
    set(CRADLE_SOURCE_IS_DIRTY TRUE)
    math(EXPR index "${index} - 1")
endif()
string(TOLOWER ${CRADLE_SOURCE_IS_DIRTY} CRADLE_SOURCE_IS_DIRTY)

# Get the commit hash.
list(GET fragments ${index} CRADLE_COMMIT_HASH)
math(EXPR index "${index} - 1")

# Get the commits since the tag.
list(GET fragments ${index} CRADLE_COMMITS_SINCE_RELEASE)

# The rest should be the tag.
list(SUBLIST fragments 0 ${index} CRADLE_VERSION)
string(JOIN "-" CRADLE_VERSION "${CRADLE_VERSION}")

# Generate the C++ code to represent all this.
configure_file("${CMAKE_CURRENT_LIST_DIR}/version_info.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/internally_generated/src/cradle/version_info.h"
    @ONLY)
