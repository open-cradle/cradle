#ifndef CRADLE_INNER_INTROSPECTION_TASKLET_UTIL_H
#define CRADLE_INNER_INTROSPECTION_TASKLET_UTIL_H

#include <iostream>
#include <ostream>

#include <cradle/inner/introspection/tasklet_info.h>

namespace cradle {

void
dump_tasklet_infos(
    tasklet_info_list const& infos, std::ostream& os = std::cout);

} // namespace cradle

#endif
