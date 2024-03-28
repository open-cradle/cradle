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
// the latter in the destructor. However, in general we cannot rely on the
// destructor being called (e.g., the blob could be stored in the memory
// cache), so an explicit on_write_completed() call is needed.
// Note that this function is synchronous (blocking).
void
blob_file_writer::on_write_completed()
{
    std::size_t const mapping_offset{0};
    std::size_t const numbytes{size_};
    bool const async{false};
    if (!region_.flush(mapping_offset, numbytes, async))
    {
        // Maybe serious enough to throw?
        auto logger{spdlog::get("cradle")};
        logger->error("blob_file_writer::on_write_completed() failed");
    }
}

blob_file_reader::blob_file_reader(file_path path) : path_{std::move(path)}
{
    auto& logger{*spdlog::get("cradle")};
    logger.info("blob_file_reader({})", path_.string());

    try
    {
        bi::file_mapping mapping{path_.c_str(), bi::read_only};
        bi::mapped_region region{mapping, bi::read_only};

        region_.swap(region);
    }
    catch (std::exception const& e)
    {
        logger.error(
            "error creating blob_file_reader for {}: {}",
            path_.string(),
            e.what());
        throw;
    }

    size_ = region_.get_size();
    data_ = reinterpret_cast<std::uint8_t*>(region_.get_address());

    logger.info("blob_file_reader(): size {}", size_);
}

} // namespace cradle
