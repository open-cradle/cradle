#include <fstream>

#include <catch2/catch.hpp>

#include <cradle/inner/blob_file/blob_file.h>
#include <cradle/inner/blob_file/blob_file_dir.h>
#include <cradle/inner/fs/file_io.h>
#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/service/config.h>

using namespace cradle;
namespace fs = std::filesystem;

static std::string cache_dir{"tests_cache"};
static fs::path cache_dir_path{cache_dir};
static fs::path cache_dir_abs_path{fs::absolute(cache_dir_path)};

static std::unique_ptr<blob_file_directory>
make_blob_file_directory()
{
    static service_config_map const config_map{
        {blob_cache_config_keys::DIRECTORY, cache_dir},
    };
    return std::make_unique<blob_file_directory>(service_config(config_map));
}

static void
touch(fs::path const& path)
{
    std::ofstream os{path.string(), std::ios::binary};
    os << '\n';
}

TEST_CASE("default blob file directory", "[blob_file]")
{
    auto dir{std::make_unique<blob_file_directory>(service_config())};

    auto path{dir->path()};
    REQUIRE(path.is_absolute());

    fs::create_directories(path);
    REQUIRE(fs::is_directory(path));
}

TEST_CASE("configured blob file directory", "[blob_file]")
{
    auto dir{make_blob_file_directory()};

    auto path{dir->path()};
    REQUIRE(path.is_absolute());

    fs::create_directories(path);
    REQUIRE(fs::is_directory(path));
}

TEST_CASE("scan non-existing blob file directory", "[blob_file]")
{
    fs::remove_all(cache_dir_path);

    auto dir{make_blob_file_directory()};

    auto next_file{dir->allocate_file()};
    REQUIRE(next_file.parent_path() == cache_dir_abs_path);
    REQUIRE(next_file.filename() == "blob_0");
}

TEST_CASE("scan prepopulated blob file directory", "[blob_file]")
{
    reset_directory(cache_dir_path);
    auto prepopulated = {"blob_3", "blob_99", "bloc_999"};
    for (auto file : prepopulated)
    {
        touch(cache_dir_path / file);
    }

    auto dir{make_blob_file_directory()};

    auto next_file{dir->allocate_file()};
    REQUIRE(next_file.parent_path() == cache_dir_abs_path);
    REQUIRE(next_file.filename() == "blob_100");
}

TEST_CASE("allocate blob file", "[blob_file]")
{
    fs::remove_all(cache_dir_path);

    auto dir{make_blob_file_directory()};

    auto blob0_file{dir->allocate_file()};
    REQUIRE(blob0_file.parent_path() == cache_dir_abs_path);
    REQUIRE(blob0_file.filename() == "blob_0");

    auto blob1_file{dir->allocate_file()};
    REQUIRE(blob1_file.parent_path() == cache_dir_abs_path);
    REQUIRE(blob1_file.filename() == "blob_1");
}

TEST_CASE("write/read blob file", "[blob_file]")
{
    auto dir{make_blob_file_directory()};
    auto path{dir->allocate_file()};

    REQUIRE(!fs::exists(path));

    auto shared_writer{std::make_shared<blob_file_writer>(path, 5)};
    REQUIRE(fs::exists(path));
    REQUIRE(fs::file_size(path) == 5);

    auto& writer{*shared_writer};
    REQUIRE(writer.maps_file());
    REQUIRE(writer.mapped_file() == path);
    REQUIRE((void*) writer.data() == (void*) writer.bytes());
    REQUIRE(writer.size() == 5);

    std::memcpy(writer.data(), "abcde", 5);
    writer.on_write_completed();

    blob writer_blob{shared_writer, writer.bytes(), writer.size()};
    REQUIRE(writer_blob.data() == writer.bytes());
    REQUIRE(writer_blob.size() == writer.size());
    REQUIRE(writer_blob.owner() == &writer);
    REQUIRE(writer_blob.mapped_file_data_owner() == &writer);

    auto shared_reader{std::make_shared<blob_file_reader>(path)};
    auto& reader{*shared_reader};
    REQUIRE(reader.maps_file());
    REQUIRE(reader.mapped_file() == path);
    REQUIRE((void*) reader.data() == (void*) reader.bytes());
    REQUIRE(reader.size() == 5);
    REQUIRE(std::memcmp(reader.data(), "abcde", 5) == 0);

    blob reader_blob{shared_reader, reader.bytes(), reader.size()};
    REQUIRE(reader_blob.data() == reader.bytes());
    REQUIRE(reader_blob.size() == reader.size());
    REQUIRE(reader_blob.owner() == &reader);
    REQUIRE(reader_blob.mapped_file_data_owner() == &reader);

    auto contents = read_file_contents(path);
    REQUIRE(contents == "abcde");
}
