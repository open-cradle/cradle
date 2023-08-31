# Create a shared library that can be loaded dynamically at runtime.
# The library implements a number of versioned requests; a newer library could provide
# requests with the same signature, but a newer implementation, and identified by newer uuids.
file(GLOB_RECURSE srcs_thinknode_v1 CONFIGURE_DEPENDS
    "thinknode-v1/*.cpp")
add_library(cradle_thinknode_v1 MODULE ${srcs_thinknode_v1})
target_link_libraries(cradle_thinknode_v1 PUBLIC
    cradle_thinknode)
