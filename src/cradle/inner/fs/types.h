#ifndef CRADLE_INNER_FS_TYPES_H
#define CRADLE_INNER_FS_TYPES_H

#include <filesystem>

namespace cradle {

// Our official type of representing paths.
// (Note that file_path is of course slightly incorrect because the path could
// refer to a directory, but it's a lot easier to read and this seems like a
// pretty common usage (e.g., Chromium does it too).)
typedef std::filesystem::path file_path;

} // namespace cradle

#endif
