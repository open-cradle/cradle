# Create a shared library containing a catalog of resolvers for Thinknode requests.
# The library can be loaded dynamically at runtime. Unless it, or a successor, has been
# loaded, serialized Thinknode requests cannot be resolved.
# The library implements a number of versioned requests; a newer library could provide
# requests with the same signature, but a newer implementation, and identified by newer uuids.
file(GLOB_RECURSE srcs_thinknode_v1 CONFIGURE_DEPENDS
    "thinknode-v1/*.cpp")
add_library(cradle_thinknode_v1 MODULE ${srcs_thinknode_v1})
target_link_libraries(cradle_thinknode_v1 PUBLIC
    cradle_thinknode)

# Copy the Thinknode DLLs to the directory containing the main DLLs.
add_custom_target(copy_thinknode_dlls
    COMMAND ${CMAKE_COMMAND} -E echo "copy DLLs to $<TARGET_FILE_DIR:cradle_thinknode>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:cradle_thinknode_v1>
    $<TARGET_FILE_DIR:cradle_thinknode>)
add_dependencies(copy_thinknode_dlls
    cradle_thinknode_v1)

# Create a C++ header file defining the location of the (copied) Thinknode DLLs.
set(thinknode_dlls_dir_file "${intgen_src_dir}/cradle/thinknode_dlls_dir.h")
add_custom_command(
    OUTPUT ${thinknode_dlls_dir_file}
    COMMAND ${CMAKE_COMMAND}
        -D input_file=${CMAKE_CURRENT_SOURCE_DIR}/cmake/thinknode_dlls_dir.h.in
        -D output_file=${thinknode_dlls_dir_file}
        -D thinknode_dlls_dir=$<TARGET_FILE_DIR:cradle_thinknode>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/thinknode_dlls_dir.cmake
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/thinknode_dlls_dir.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/thinknode_dlls_dir.h.in)
add_custom_target(thinknode_dlls_dir_target DEPENDS ${thinknode_dlls_dir_file})
