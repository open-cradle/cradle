#include <fstream>
#include <iostream>

#include <boost/interprocess/file_mapping.hpp>
#include <spdlog/spdlog.h>

#include <cradle/inner/blob_file/blob_file.h>

namespace cradle {

namespace bi = boost::interprocess;

blob_file_writer::blob_file_writer(file_path path, std::size_t size)
    : path_{std::move(path)}, size_{size}
{
    spdlog::get("cradle")->info("creating blob file {}", path_.string());
    {
        // Create the file with the given size
        std::filebuf fbuf;
        fbuf.open(
            path_.string(),
            std::ios_base::in | std::ios_base::out | std::ios_base::trunc
                | std::ios_base::binary);
        fbuf.pubseekoff(size - 1, std::ios_base::beg);
        fbuf.sputc(0);
    }

    bi::file_mapping mapping{path_.c_str(), bi::read_write};
    bi::mapped_region region{mapping, bi::read_write};

    region_.swap(region);

    data_ = reinterpret_cast<std::uint8_t*>(region_.get_address());
}

// On Linux, changes to memory are guaranteed to be written to the file system
// only on msync(2) or munmap(2) calls. The former happens in this function,
// the latter in the destructor. Note that this function is asynchronous.
void
blob_file_writer::on_write_completed()
{
    region_.flush();
}

blob_file_reader::blob_file_reader(file_path path) : path_{std::move(path)}
{
    spdlog::get("cradle")->info("blob_file_reader({})", path_.string());

    bi::file_mapping mapping{path_.c_str(), bi::read_only};
    bi::mapped_region region{mapping, bi::read_only};

    region_.swap(region);

    size_ = region_.get_size();
    data_ = reinterpret_cast<std::uint8_t*>(region_.get_address());

    spdlog::get("cradle")->info("blob_file_reader(): size {}", size_);
}

} // namespace cradle
