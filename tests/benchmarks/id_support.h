#include <cradle/inner/core/id.h>

namespace cradle {

// Returns an id_interface for some type
id_interface const&
get_my_struct_id0();

// Returns an id_interface for some similar but different type
id_interface const&
get_my_struct_id1();

} // namespace cradle
