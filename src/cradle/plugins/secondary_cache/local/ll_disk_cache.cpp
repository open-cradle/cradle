#include <cradle/plugins/secondary_cache/local/ll_disk_cache.h>

#include <chrono>
#include <filesystem>
#include <mutex>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <sqlite3.h>

#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/fs/app_dirs.h>
#include <cradle/inner/fs/utilities.h>
#include <cradle/inner/utilities/errors.h>
#include <cradle/inner/utilities/logging.h>
#include <cradle/inner/utilities/text.h>

namespace cradle {

struct ll_disk_cache_impl
{
    file_path dir;

    sqlite3* db = nullptr;

    // prepared statements
    sqlite3_stmt* database_version_query = nullptr;

    sqlite3_stmt* insert_ac_entry_statement = nullptr;
    sqlite3_stmt* ac_lookup_query = nullptr;
    sqlite3_stmt* get_cas_id_from_ac_query = nullptr;
    sqlite3_stmt* ac_entry_count_query = nullptr;
    sqlite3_stmt* ac_lru_entry_list_query = nullptr;
    sqlite3_stmt* record_ac_usage_statement = nullptr;
    sqlite3_stmt* remove_ac_entry_statement = nullptr;

    sqlite3_stmt* cas_insert_statement = nullptr;
    sqlite3_stmt* initiate_cas_insert_statement = nullptr;
    sqlite3_stmt* finish_cas_insert_statement = nullptr;
    sqlite3_stmt* cas_lookup_by_digest_query = nullptr;
    sqlite3_stmt* cas_lookup_query = nullptr;
    sqlite3_stmt* cas_entry_count_query = nullptr;
    sqlite3_stmt* total_cas_size_query = nullptr;
    sqlite3_stmt* cas_entry_list_query = nullptr;
    sqlite3_stmt* count_cas_entry_refs_query = nullptr;
    sqlite3_stmt* remove_cas_entry_statement = nullptr;

    int64_t size_limit;

    // used to track when we need to check if the cache is too big
    int64_t bytes_inserted_since_last_sweep = 0;

    // Used for detecting an idle period
    std::chrono::time_point<std::chrono::system_clock> latest_activity;

    // ac_id's for actions records that were read, but whose usage has not
    // been written to the database. This container should not contain
    // duplicates, but replacing the vector with a set or unordered_set makes
    // look-up measurably slower.
    std::vector<int64_t> ac_ids_to_flush;

    // Protects all access to the cache. The ll_disk_cache member functions
    // lock this mutex; other functions may assume it's locked.
    std::mutex mutex;

    std::shared_ptr<spdlog::logger> logger;
};

// SQLITE UTILITIES

static void
open_db(sqlite3** db, file_path const& file)
{
    spdlog::get("cradle")->info("Using disk cache {}", file.string());
    // sqlite3_open() apparently is successful even if file is not an SQLite
    // database.
    if (sqlite3_open(file.string().c_str(), db) != SQLITE_OK)
    {
        CRADLE_THROW(
            ll_disk_cache_failure()
            << ll_disk_cache_path_info(file.parent_path())
            << internal_error_message_info(
                   "failed to create disk cache index file (index.db)"));
    }
}

// Returns a short (one-line) message for an exception.
// All CRADLE_THROW instances in this file define an
// internal_error_message_info string containing a one-line error message,
// which is suitable to be returned here.
// These exceptions' what() returns a many-line message that is way too long to
// include in a logger's message.
// Most CRADLE_THROW's here are of the "cannot happen" internal error kind.
static std::string
what(std::exception const& e)
{
    if (auto const* msg
        = boost::get_error_info<internal_error_message_info>(e))
    {
        return *msg;
    }
    return e.what();
}

static void
throw_for_code(
    ll_disk_cache_impl const& cache, int code, std::string const& prefix)
{
    std::string code_text{sqlite3_errstr(code)};
    std::string msg_text{sqlite3_errmsg(cache.db)};
    std::string all_text{
        fmt::format("{} ({}): {}", prefix, code_text, msg_text)};
    CRADLE_THROW(
        ll_disk_cache_failure() << ll_disk_cache_path_info(cache.dir)
                                << internal_error_message_info(all_text));
}

static void
throw_for_code(int code, std::string const& prefix)
{
    std::string code_text{sqlite3_errstr(code)};
    std::string all_text{fmt::format("{} ({})", prefix, code_text)};
    CRADLE_THROW(
        ll_disk_cache_failure() << internal_error_message_info(all_text));
}

static void
throw_query_error(
    ll_disk_cache_impl const& cache,
    std::string const& sql,
    std::string const& error)
{
    CRADLE_THROW(
        ll_disk_cache_failure()
        << ll_disk_cache_path_info(cache.dir)
        << internal_error_message_info(fmt::format(
               "error executing SQL query in index.db\n"
               "SQL query: {}\n"
               "error: {}",
               sql,
               error)));
}

static std::string
copy_and_free_message(char* msg)
{
    if (msg)
    {
        std::string s = msg;
        sqlite3_free(msg);
        return s;
    }
    else
    {
        return "";
    }
}

static void
execute_sql(ll_disk_cache_impl const& cache, std::string const& sql)
{
    char* msg;
    int code = sqlite3_exec(cache.db, sql.c_str(), 0, 0, &msg);
    std::string error = copy_and_free_message(msg);
    if (code != SQLITE_OK)
    {
        throw_query_error(cache, sql, error);
    }
}

// Check a return code from SQLite.
static void
check_sqlite_code(int code)
{
    if (code != SQLITE_OK)
    {
        throw_for_code(code, "SQLite error");
    }
}

// Create a prepared statement.
// This checks to make sure that the creation was successful, so the returned
// pointer is always valid.
static sqlite3_stmt*
prepare_statement(ll_disk_cache_impl const& cache, std::string const& sql)
{
    sqlite3_stmt* statement;
    auto code = sqlite3_prepare_v2(
        cache.db,
        sql.c_str(),
        boost::numeric_cast<int>(sql.length()),
        &statement,
        nullptr);
    if (code != SQLITE_OK)
    {
        throw_for_code(
            cache, code, fmt::format("error preparing SQL query {}", sql));
    }
    return statement;
}

// Bind a 64-bit integer to a parameter of a prepared statement.
static void
bind_int64(sqlite3_stmt* statement, int parameter_index, int64_t value)
{
    check_sqlite_code(sqlite3_bind_int64(statement, parameter_index, value));
}

// Bind a string to a parameter of a prepared statement.
static void
bind_string(
    sqlite3_stmt* statement, int parameter_index, std::string const& value)
{
    check_sqlite_code(sqlite3_bind_text64(
        statement,
        parameter_index,
        value.c_str(),
        value.size(),
        SQLITE_STATIC,
        SQLITE_UTF8));
}

// Bind a blob to a parameter of a prepared statement.
static void
bind_blob(sqlite3_stmt* statement, int parameter_index, blob const& value)
{
    check_sqlite_code(sqlite3_bind_blob64(
        statement,
        parameter_index,
        value.data(),
        value.size(),
        SQLITE_STATIC));
}

// Execute a prepared statement (with variables already bound to it) and check
// that it finished successfully.
// This should only be used for statements that don't return results.
static void
execute_prepared_statement(
    ll_disk_cache_impl const& cache, sqlite3_stmt* statement)
{
    // cache.logger->debug("execute_prepared_statement simple");
    auto code = sqlite3_step(statement);
    if (code != SQLITE_DONE)
    {
        throw_for_code(cache, code, "SQL query failed");
    }
    check_sqlite_code(sqlite3_reset(statement));
    // Strings and blobs are bound with SQLITE_STATIC, promising the passed
    // pointer remains valid until either:
    // - The prepared statement is finalized; or
    // - The statement's parameter is rebound (or its binding is cleared).
    check_sqlite_code(sqlite3_clear_bindings(statement));
}

// ROWS

struct sqlite_row
{
    sqlite3_stmt* statement;
};

static bool
has_value(sqlite_row& row, int column_index)
{
    return sqlite3_column_type(row.statement, column_index) != SQLITE_NULL;
}

static bool
read_bool(sqlite_row& row, int column_index)
{
    return sqlite3_column_int(row.statement, column_index) != 0;
}

static int
read_int32(sqlite_row& row, int column_index)
{
    return sqlite3_column_int(row.statement, column_index);
}

static int64_t
read_int64(sqlite_row& row, int column_index)
{
    return sqlite3_column_int64(row.statement, column_index);
}

static std::string
read_string(sqlite_row& row, int column_index)
{
    return reinterpret_cast<char const*>(
        sqlite3_column_text(row.statement, column_index));
}

static blob
read_blob(sqlite_row& row, int column_index)
{
    auto const* data = sqlite3_column_blob(row.statement, column_index);
    auto size = sqlite3_column_bytes(row.statement, column_index);
    auto const* begin = reinterpret_cast<std::uint8_t const*>(data);
    auto const* end = begin + size;
    return make_blob(byte_vector{begin, end});
}

// Execute a prepared statement (with variables already bound to it), pass all
// the rows from the result set into the supplied callback, and check that the
// query finishes successfully.
// This could be made into a non-template function by passing row_handler in a
// function_view, making the object size smaller, but measurably slower.
struct expected_column_count
{
    int value;
};
struct single_row_result
{
    bool value;
};
template<class RowHandler>
static void
execute_prepared_statement(
    ll_disk_cache_impl const& cache,
    sqlite3_stmt* statement,
    expected_column_count expected_columns,
    single_row_result single_row,
    RowHandler const& row_handler)
{
    // cache.logger->debug("execute_prepared_statement extended");
    int row_count = 0;
    int code;
    while (true)
    {
        code = sqlite3_step(statement);
        if (code == SQLITE_ROW)
        {
            if (sqlite3_column_count(statement) != expected_columns.value)
            {
                CRADLE_THROW(
                    ll_disk_cache_failure()
                    << ll_disk_cache_path_info(cache.dir)
                    << internal_error_message_info(std::string(
                           "SQL query result column count incorrect\n")));
            }
            sqlite_row row;
            row.statement = statement;
            row_handler(row);
            ++row_count;
        }
        else
        {
            break;
        }
    }
    if (code != SQLITE_DONE)
    {
        throw_for_code(cache, code, "SQL query failed");
    }
    if (single_row.value && row_count != 1)
    {
        CRADLE_THROW(
            ll_disk_cache_failure()
            << ll_disk_cache_path_info(cache.dir)
            << internal_error_message_info(
                   std::string("SQL query row count incorrect\n")));
    }
    check_sqlite_code(sqlite3_reset(statement));
}

// OPERATIONS ON THE AC

static void
record_ac_usage(ll_disk_cache_impl& cache, int64_t ac_id)
{
    auto* stmt = cache.record_ac_usage_statement;
    bind_int64(stmt, 1, ac_id);
    execute_prepared_statement(cache, stmt);
}

static void
flush_ac_usage(ll_disk_cache_impl& cache)
{
    cache.logger->info(
        "flush_ac_usage ({} items)", cache.ac_ids_to_flush.size());
    // An alternative would be a single
    //   UPDATE actions SET ... WHERE ac_id in (...)
    // but this happens to be slower than performing a query for each ac_id.
    for (auto ac_id : cache.ac_ids_to_flush)
    {
        record_ac_usage(cache, ac_id);
    }
    cache.ac_ids_to_flush.clear();
}

static bool
should_flush_ac_usage(ll_disk_cache_impl const& cache)
{
    if (cache.ac_ids_to_flush.empty())
    {
        // Nothing to do
        return false;
    }
    if (cache.ac_ids_to_flush.size() >= 10)
    {
        // Limit the size of the backlog, and the duration of the update
        return true;
    }
    if (std::chrono::system_clock::now() - cache.latest_activity
        > std::chrono::seconds(1))
    {
        // The disk cache seems to be idle.
        return true;
    }
    return false;
}

static void
insert_ac_entry(
    ll_disk_cache_impl& cache, std::string const& ac_key, int64_t cas_id)
{
    auto* stmt = cache.insert_ac_entry_statement;
    bind_string(stmt, 1, ac_key);
    bind_int64(stmt, 2, cas_id);
    execute_prepared_statement(cache, stmt);
}

// Returns (ac_id, cas_id) pair for the specified AC entry, or nullopt if no
// such entry
static std::optional<std::pair<int64_t, int64_t>>
look_up_ac_and_cas_ids(ll_disk_cache_impl& cache, std::string const& ac_key)
{
    auto* stmt = cache.ac_lookup_query;
    bind_string(stmt, 1, ac_key);
    bool exists = false;
    int64_t ac_id{};
    int64_t cas_id{};
    execute_prepared_statement(
        cache,
        stmt,
        expected_column_count{2},
        single_row_result{false},
        [&](sqlite_row& row) {
            ac_id = read_int64(row, 0);
            cas_id = read_int64(row, 1);
            exists = true;
        });
    if (!exists)
    {
        return std::nullopt;
    }

    // Add ac_id to ac_ids_to_flush, ensuring no duplicates appear. In a
    // production environment, the memory cache will (or should) already ensure
    // this, but benchmark tests that measure just disk cache performance
    // do not.
    // A compromise is to first check on ac_id's presence. This has no
    // measurable performance impact, and prevents negative impact on benchmark
    // output.
    if (std::find(
            cache.ac_ids_to_flush.begin(), cache.ac_ids_to_flush.end(), ac_id)
        == cache.ac_ids_to_flush.end())
    {
        cache.ac_ids_to_flush.push_back(ac_id);
    }

    return std::make_pair(ac_id, cas_id);
}

static std::optional<int64_t>
look_up_ac_id(ll_disk_cache_impl& cache, std::string const& ac_key)
{
    auto opt_id_pair = look_up_ac_and_cas_ids(cache, ac_key);
    if (!opt_id_pair)
    {
        return std::nullopt;
    }
    return std::get<0>(*opt_id_pair);
}

static std::optional<int64_t>
look_up_cas_id(ll_disk_cache_impl& cache, std::string const& ac_key)
{
    auto opt_id_pair = look_up_ac_and_cas_ids(cache, ac_key);
    if (!opt_id_pair)
    {
        return std::nullopt;
    }
    return std::get<1>(*opt_id_pair);
}

// Returns the cas_id from the AC entry for ac_id.
static int64_t
get_cas_id_for_ac_entry(ll_disk_cache_impl const& cache, int64_t ac_id)
{
    auto* stmt = cache.get_cas_id_from_ac_query;
    bind_int64(stmt, 1, ac_id);
    int64_t cas_id = 0;
    execute_prepared_statement(
        cache,
        stmt,
        expected_column_count{1},
        single_row_result{true},
        [&](sqlite_row& row) { cas_id = read_int64(row, 0); });
    return cas_id;
}

// Get the number of entries in the AC.
static int64_t
get_ac_entry_count(ll_disk_cache_impl& cache)
{
    int64_t count;
    execute_prepared_statement(
        cache,
        cache.ac_entry_count_query,
        expected_column_count{1},
        single_row_result{true},
        [&](sqlite_row& row) { count = read_int64(row, 0); });
    return count;
}

struct lru_entry_t
{
    int64_t ac_id;
    int64_t cas_id;
};
using lru_entry_list_t = std::vector<lru_entry_t>;

// Get a list of entries in the AC in LRU order.
static lru_entry_list_t
get_ac_lru_entries(ll_disk_cache_impl& cache)
{
    lru_entry_list_t entries;
    execute_prepared_statement(
        cache,
        cache.ac_lru_entry_list_query,
        expected_column_count{2},
        single_row_result{false},
        [&](sqlite_row& row) {
            lru_entry_t entry{
                .ac_id = read_int64(row, 0), .cas_id = read_int64(row, 1)};
            entries.push_back(entry);
        });
    return entries;
}

static void
remove_ac_entry(ll_disk_cache_impl& cache, int64_t ac_id)
{
    cache.logger->debug(" remove AC entry {}", ac_id);
    auto* stmt = cache.remove_ac_entry_statement;
    bind_int64(stmt, 1, ac_id);
    execute_prepared_statement(cache, stmt);
}

// OPERATIONS ON THE CAS (DB ONLY)

// Inserts a complete entry in the CAS, returning its cas_id
static int64_t
insert_cas_entry(
    ll_disk_cache_impl const& cache,
    std::string const& digest,
    blob const& value,
    std::size_t original_size)
{
    cache.logger->debug(
        " insert_cas_entry: digest {}, original_size {}",
        digest,
        original_size);
    auto* stmt = cache.cas_insert_statement;
    bind_string(stmt, 1, digest);
    bind_blob(stmt, 2, value);
    bind_int64(stmt, 3, value.size());
    bind_int64(stmt, 4, original_size);
    execute_prepared_statement(cache, stmt);
    // Alternative: use a RETURNING clause
    auto cas_id = sqlite3_last_insert_rowid(cache.db);
    if (cas_id == 0)
    {
        // Since we checked that the insert succeeded, we really shouldn't
        // get here.
        CRADLE_THROW(
            ll_disk_cache_failure()
            << ll_disk_cache_path_info(cache.dir)
            << internal_error_message_info(
                   "failed to create entry in index.db"));
    }
    return cas_id;
}

// Inserts an incomplete / invalid entry in the CAS, returning its cas_id.
static int64_t
initiate_cas_insert(ll_disk_cache_impl const& cache, std::string const& digest)
{
    auto* stmt = cache.initiate_cas_insert_statement;
    bind_string(stmt, 1, digest);
    execute_prepared_statement(cache, stmt);
    // Get the ID that was inserted.
    auto cas_id = sqlite3_last_insert_rowid(cache.db);
    if (cas_id == 0)
    {
        // Since we checked that the insert succeeded, we really shouldn't
        // get here.
        CRADLE_THROW(
            ll_disk_cache_failure()
            << ll_disk_cache_path_info(cache.dir)
            << internal_error_message_info(
                   "failed to create entry in index.db"));
    }
    return cas_id;
}

// Finalizes a CAS entry that was inserted via initiate_cas_insert().
static void
finish_cas_insert(
    ll_disk_cache_impl const& cache,
    int64_t cas_id,
    std::size_t size,
    std::size_t original_size)
{
    auto* stmt = cache.finish_cas_insert_statement;
    bind_int64(stmt, 1, size);
    bind_int64(stmt, 2, original_size);
    bind_int64(stmt, 3, cas_id);
    execute_prepared_statement(cache, stmt);
}

static std::optional<int64_t>
look_up_cas_id_by_digest(
    ll_disk_cache_impl const& cache, std::string const& digest)
{
    auto* stmt = cache.cas_lookup_by_digest_query;
    bind_string(stmt, 1, digest);
    bool exists = false;
    int64_t cas_id{};
    execute_prepared_statement(
        cache,
        stmt,
        expected_column_count{1},
        single_row_result{false},
        [&](sqlite_row& row) {
            cas_id = read_int64(row, 0);
            exists = true;
        });
    if (!exists)
    {
        return std::nullopt;
    }
    return cas_id;
}

// internal_cas_entry_t differs from ll_disk_cache_cas_entry by having an extra
// "valid" field.
struct internal_cas_entry_t
{
    int64_t cas_id;
    std::string digest;
    bool valid;
    bool in_db;
    std::optional<blob> value;
    int64_t size;
    int64_t original_size;
};

static internal_cas_entry_t
look_up_internal_cas_entry(ll_disk_cache_impl const& cache, int64_t cas_id)
{
    auto* stmt = cache.cas_lookup_query;
    bind_int64(stmt, 1, cas_id);
    internal_cas_entry_t entry{};
    execute_prepared_statement(
        cache,
        stmt,
        expected_column_count{6},
        single_row_result{true},
        [&](sqlite_row& row) {
            entry.cas_id = cas_id;
            entry.digest = read_string(row, 0);
            entry.valid = read_bool(row, 1);
            entry.in_db = read_bool(row, 2);
            entry.value = has_value(row, 3)
                              ? std::make_optional(read_blob(row, 3))
                              : std::nullopt;
            entry.size = has_value(row, 4) ? read_int64(row, 4) : 0;
            entry.original_size = has_value(row, 5) ? read_int64(row, 5) : 0;
        });
    return entry;
}

static std::optional<ll_disk_cache_cas_entry>
look_up_cas_entry(ll_disk_cache_impl const& cache, int64_t cas_id)
{
    auto internal_entry = look_up_internal_cas_entry(cache, cas_id);
    if (!internal_entry.valid)
    {
        return std::nullopt;
    }
    return ll_disk_cache_cas_entry{
        .cas_id = internal_entry.cas_id,
        .digest = std::move(internal_entry.digest),
        .in_db = internal_entry.in_db,
        .value = std::move(internal_entry.value),
        .size = internal_entry.size,
        .original_size = internal_entry.original_size};
}

// Get the number of entries in the CAS.
static int64_t
get_cas_entry_count(ll_disk_cache_impl& cache)
{
    int64_t count{};
    execute_prepared_statement(
        cache,
        cache.cas_entry_count_query,
        expected_column_count{1},
        single_row_result{true},
        [&](sqlite_row& row) { count = read_int64(row, 0); });
    return count;
}

// Returns the total size of all entries in the CAS.
static int64_t
get_total_cas_size(ll_disk_cache_impl& cache)
{
    int64_t size{};
    execute_prepared_statement(
        cache,
        cache.total_cas_size_query,
        expected_column_count{1},
        single_row_result{true},
        [&](sqlite_row& row) { size = read_int64(row, 0); });
    return size;
}

// Returns a list of valid entries in the CAS.
// The "value" members are left as std::nullopt.
static std::vector<ll_disk_cache_cas_entry>
get_cas_entry_list(ll_disk_cache_impl& cache)
{
    std::vector<ll_disk_cache_cas_entry> entries;
    execute_prepared_statement(
        cache,
        cache.cas_entry_list_query,
        expected_column_count{5},
        single_row_result{false},
        [&](sqlite_row& row) {
            ll_disk_cache_cas_entry e;
            e.cas_id = read_int64(row, 0);
            e.digest = read_string(row, 1);
            e.in_db = read_bool(row, 2);
            e.size = has_value(row, 3) ? read_int64(row, 3) : 0;
            e.original_size = has_value(row, 4) ? read_int64(row, 4) : 0;
            entries.push_back(e);
        });
    return entries;
}

// Returns the number of AC records referring to the specified CAS record.
static int64_t
count_cas_entry_refs(ll_disk_cache_impl& cache, int64_t cas_id)
{
    auto* stmt = cache.count_cas_entry_refs_query;
    bind_int64(stmt, 1, cas_id);
    int64_t count{};
    execute_prepared_statement(
        cache,
        stmt,
        expected_column_count{1},
        single_row_result{true},
        [&](sqlite_row& row) { count = read_int64(row, 0); });
    cache.logger->debug(" count_cas_entry_refs({}) -> {}", cas_id, count);
    return count;
}

static void
remove_cas_entry_db_only(ll_disk_cache_impl& cache, int64_t cas_id)
{
    auto* stmt = cache.remove_cas_entry_statement;
    bind_int64(stmt, 1, cas_id);
    execute_prepared_statement(cache, stmt);
}

// OPERATIONS ON THE CAS (FILE ONLY)

static file_path
get_path_for_digest(ll_disk_cache_impl& cache, std::string const& digest)
{
    // The digest is used as filename.
    return cache.dir / digest;
}

// OPERATIONS ON THE CAS (DB AND FILE)

// Removes the given CAS entry from the database, and removes the corresponding
// file if any.
// Returns the size of the removed CAS entry.
static int64_t
remove_cas_entry_db_and_file(ll_disk_cache_impl& cache, int64_t cas_id)
{
    auto entry = look_up_internal_cas_entry(cache, cas_id);
    auto size_diff = entry.size;
    remove_cas_entry_db_only(cache, cas_id);
    if (!entry.in_db)
    {
        auto path{get_path_for_digest(cache, entry.digest)};
        if (exists(path))
        {
            remove(path);
        }
    }
    return size_diff;
}

// OPERATIONS ON COMBINED AC AND CAS

// Returns the CAS entry, if any, associated with a particular AC key.
//
// There are several possibilities:
// - The entry is not in the AC: return nullopt.
// - The entry exists in the AC and the value is in the database:
//   return an object with value set to something non-nullopt.
// - The entry exists in the AC and the value is in a file:
//   return an object with value set to nullopt.
// - The entry exists in the AC, the value will be in a file, but the write has
//   not finished: return nullopt.
static std::optional<ll_disk_cache_cas_entry>
look_up(ll_disk_cache_impl& cache, std::string const& ac_key)
{
    auto opt_cas_id = look_up_cas_id(cache, ac_key);
    if (!opt_cas_id)
    {
        return std::nullopt;
    }
    return look_up_cas_entry(cache, *opt_cas_id);
}

// Removes the specified AC entry, and the CAS entry it refers to if this is
// the last reference. Returns the size of the removed CAS entry, or 0 if none
// was removed.
static int64_t
remove_ac_entry_with_cas_entry(
    ll_disk_cache_impl& cache, int64_t ac_id, int64_t cas_id)
{
    int64_t size_diff{0};
    remove_ac_entry(cache, ac_id);
    if (count_cas_entry_refs(cache, cas_id) == 0)
    {
        cache.logger->info(" removing stale CAS entry {}", cas_id);
        size_diff = remove_cas_entry_db_and_file(cache, cas_id);
        cache.logger->info(" reclaimed {} bytes", size_diff);
    }
    return size_diff;
}

// Removes the specified AC entry, and the CAS entry it refers to if this is
// the last reference.
static void
remove_ac_entry_with_cas_entry(ll_disk_cache_impl& cache, int64_t ac_id)
{
    int64_t cas_id{get_cas_id_for_ac_entry(cache, ac_id)};
    remove_ac_entry_with_cas_entry(cache, ac_id, cas_id);
}

static void
remove_all_entries(ll_disk_cache_impl& cache)
{
    for (auto const& entry : get_ac_lru_entries(cache))
    {
        try
        {
            remove_ac_entry_with_cas_entry(cache, entry.ac_id, entry.cas_id);
        }
        catch (std::exception const& e)
        {
            cache.logger->error(
                "Error removing entries {}/{}: {}",
                entry.ac_id,
                entry.cas_id,
                what(e));
        }
    }
}

// Removes invalid CAS entries, and any AC entries referring to them.
//
// If an initiate_insert() is not followed up by a finish_insert() (e.g.
// because the process got killed), we're left with an invalid CAS entry, and
// AC entries referring to that. A new initiate_insert() attempt would assume
// that someone else is still finishing the insert, and not try to remedy the
// situation.
// The solution is to have cache initialization remove invalid entries, so that
// a new initiate_insert() can proceed.
static void
remove_invalid_entries(ll_disk_cache_impl& cache)
{
    cache.logger->info("deleting invalid entries");
    // Delete AC entries referring to an invalid CAS entry.
    execute_sql(
        cache,
        "delete from actions where cas_id in"
        " (select cas_id from cas where not valid);");
    // Delete the invalid CAS entries themselves.
    execute_sql(cache, "delete from cas where not valid;");
}

// OTHER UTILITIES

static void
enforce_cache_size_limit(ll_disk_cache_impl& cache)
{
    try
    {
        int64_t size = get_total_cas_size(cache);
        if (size > cache.size_limit)
        {
            for (auto const& i : get_ac_lru_entries(cache))
            {
                try
                {
                    auto size_diff = remove_ac_entry_with_cas_entry(
                        cache, i.ac_id, i.cas_id);
                    size -= size_diff;
                    if (size <= cache.size_limit)
                    {
                        break;
                    }
                }
                catch (std::exception const& e)
                {
                    cache.logger->error(
                        "Error removing entries {}/{}: {}",
                        i.ac_id,
                        i.cas_id,
                        what(e));
                }
            }
        }
        cache.bytes_inserted_since_last_sweep = 0;
    }
    catch (std::exception const& e)
    {
        cache.logger->error("enforce_cache_size_limit() caught {}", what(e));
    }
}

static void
record_activity(ll_disk_cache_impl& cache)
{
    cache.latest_activity = std::chrono::system_clock::now();
}

static void
record_cache_growth(ll_disk_cache_impl& cache, uint64_t size)
{
    cache.bytes_inserted_since_last_sweep += size;
    // Allow the cache to write out roughly 1% of its capacity between size
    // checks. (So it could exceed its limit slightly, but only temporarily,
    // and not by much.)
    // Size checks on the database could also be avoided by locally keeping
    // track of total CAS size.
    if (cache.bytes_inserted_since_last_sweep > cache.size_limit / 0x80)
    {
        enforce_cache_size_limit(cache);
    }
}

static void
shut_down(ll_disk_cache_impl& cache)
{
    if (cache.db)
    {
        sqlite3_finalize(cache.database_version_query);

        sqlite3_finalize(cache.insert_ac_entry_statement);
        sqlite3_finalize(cache.ac_lookup_query);
        sqlite3_finalize(cache.get_cas_id_from_ac_query);
        sqlite3_finalize(cache.ac_entry_count_query);
        sqlite3_finalize(cache.ac_lru_entry_list_query);
        sqlite3_finalize(cache.record_ac_usage_statement);
        sqlite3_finalize(cache.remove_ac_entry_statement);

        sqlite3_finalize(cache.cas_insert_statement);
        sqlite3_finalize(cache.initiate_cas_insert_statement);
        sqlite3_finalize(cache.finish_cas_insert_statement);
        sqlite3_finalize(cache.cas_lookup_by_digest_query);
        sqlite3_finalize(cache.cas_lookup_query);
        sqlite3_finalize(cache.cas_entry_count_query);
        sqlite3_finalize(cache.total_cas_size_query);
        sqlite3_finalize(cache.cas_entry_list_query);
        sqlite3_finalize(cache.count_cas_entry_refs_query);
        sqlite3_finalize(cache.remove_cas_entry_statement);

        sqlite3_close(cache.db);
        cache.db = nullptr;
    }
}

// Open (or create) the database file and verify that the version number is
// what we expect.
static void
open_and_check_db(ll_disk_cache_impl& cache)
{
    int const expected_database_version = 4;

    open_db(&cache.db, cache.dir / "index.db");

    // Get the version number embedded in the database.
    cache.database_version_query
        = prepare_statement(cache, "pragma user_version;");
    int database_version{};
    execute_prepared_statement(
        cache,
        cache.database_version_query,
        expected_column_count{1},
        single_row_result{true},
        [&](sqlite_row& row) { database_version = read_int32(row, 0); });

    // A database_version of 0 indicates a fresh database, so initialize it.
    if (database_version == 0)
    {
        cache.logger->info("creating tables on fresh database");
        // Create the CAS part of the cache
        execute_sql(
            cache,
            "create table cas("
            " cas_id integer primary key,"
            " digest text unique not null,"
            " valid boolean not null,"
            " in_db boolean not null,"
            " value blob," // used only if in_db
            " size integer,"
            " original_size integer);");
        // Create the AC part of the cache
        execute_sql(
            cache,
            "create table actions("
            " ac_id integer primary key,"
            " key text unique not null,"
            " cas_id integer not null,"
            " last_accessed datetime);");
        execute_sql(
            cache,
            fmt::format(
                "pragma user_version = {};", expected_database_version));

        // An index might improve performance, but so far benchmarks don't show
        // it.
        // Try to speed up key lookup (ac_lookup_query); used every time the
        // cache is consulted:
        // execute_sql(cache,
        //   "create unique index actions_key on actions(key);");
        // Try to speed up count_cas_entry_refs_query; used when an actions
        // entry is deleted:
        // execute_sql(cache,
        //   "create index actions_cas_id on actions(cas_id);");
    }
    // If we find a database from a different version, abort.
    else if (database_version != expected_database_version)
    {
        CRADLE_THROW(
            ll_disk_cache_failure()
            << ll_disk_cache_path_info(cache.dir)
            << internal_error_message_info("incompatible database"));
    }
}

static void
initialize(ll_disk_cache_impl& cache, ll_disk_cache_config const& config)
{
    cache.dir = config.directory
                    ? file_path(*config.directory)
                    : get_shared_cache_dir(std::nullopt, "cradle");
    cache.size_limit = config.size_limit.value_or(0x40'00'00'00);
    cache.logger = ensure_logger("ll_disk_cache");

    // Prepare the directory.
    if (config.start_empty)
    {
        reset_directory(cache.dir);
    }
    else if (!exists(cache.dir))
    {
        create_directory(cache.dir);
    }

    // Open the database file.
    try
    {
        open_and_check_db(cache);
    }
    catch (std::exception const& e)
    {
        cache.logger->error("Error opening database: {}. Retrying.", what(e));
        // If the first attempt fails, we may have an incompatible or corrupt
        // database, so shut everything down, clear out the directory, and try
        // again.
        shut_down(cache);
        reset_directory(cache.dir);
        open_and_check_db(cache);
    }

    // Set various performance tuning flags.

    // Somewhat dangerous in case of an OS crash or power loss.
    // Much much faster than FULL or NORMAL unless combined with WAL.
    execute_sql(cache, "pragma synchronous = off;");

    // Much faster than NORMAL
    execute_sql(cache, "pragma locking_mode = exclusive;");

    // Dangerous: if the application crashes in the middle of a transaction,
    // then the database file will very likely go corrupt.
    // WAL is safer but slower, and removes the need for the flush_ac_usage
    // mechanism.
    execute_sql(cache, "pragma journal_mode = memory;");

    // Initialize our prepared statements.
    cache.insert_ac_entry_statement = prepare_statement(
        cache,
        "insert into actions"
        " (key, cas_id, last_accessed)"
        " values(?1, ?2, strftime('%Y-%m-%d %H:%M:%f', 'now'));");
    cache.ac_lookup_query = prepare_statement(
        cache, "select ac_id, cas_id from actions where key=?1;");
    cache.get_cas_id_from_ac_query = prepare_statement(
        cache, "select cas_id from actions where ac_id=?1;");
    cache.ac_entry_count_query
        = prepare_statement(cache, "select count(*) from actions;");
    cache.ac_lru_entry_list_query = prepare_statement(
        cache,
        "select ac_id, cas_id from actions"
        " order by last_accessed;");
    cache.record_ac_usage_statement = prepare_statement(
        cache,
        "update actions set last_accessed=strftime('%Y-%m-%d %H:%M:%f', 'now')"
        " where ac_id=?1;");
    cache.remove_ac_entry_statement
        = prepare_statement(cache, "delete from actions where ac_id=?1;");

    cache.cas_insert_statement = prepare_statement(
        cache,
        "insert into cas(digest, valid, in_db, value, size, original_size) "
        "values (?1, 1, 1, ?2, ?3, ?4);");
    cache.initiate_cas_insert_statement = prepare_statement(
        cache, "insert into cas(digest, valid, in_db) values (?1, 0, 0);");
    cache.finish_cas_insert_statement = prepare_statement(
        cache,
        "update cas set valid=1, in_db=0, size=?1, original_size=?2"
        " where cas_id=?3;");
    cache.cas_lookup_by_digest_query
        = prepare_statement(cache, "select cas_id from cas where digest=?1;");
    cache.cas_lookup_query = prepare_statement(
        cache,
        "select digest, valid, in_db, value, size, original_size"
        " from cas where cas_id=?1;");
    cache.cas_entry_count_query
        = prepare_statement(cache, "select count(*) from cas;");
    cache.total_cas_size_query
        = prepare_statement(cache, "select sum(size) from cas;");
    cache.cas_entry_list_query = prepare_statement(
        cache,
        "select cas_id, digest, in_db, size, original_size from cas"
        " where valid=1;");
    cache.count_cas_entry_refs_query = prepare_statement(
        cache, "select count(*) from actions where cas_id=?1;");
    cache.remove_cas_entry_statement
        = prepare_statement(cache, "delete from cas where cas_id=?1;");

    if (config.start_empty)
    {
        remove_all_entries(cache);
    }
    // Do initial housekeeping.
    remove_invalid_entries(cache);
    record_activity(cache);
    enforce_cache_size_limit(cache);
}

// API

ll_disk_cache::ll_disk_cache(ll_disk_cache_config const& config)
    : impl_(new ll_disk_cache_impl)
{
    this->reset(config);
}

ll_disk_cache::ll_disk_cache(ll_disk_cache&& other) = default;

ll_disk_cache::~ll_disk_cache()
{
    // This could be a moved-from object. If so, the destructor is the only
    // allowed API call.
    if (this->impl_)
    {
        shut_down(*this->impl_);
    }
}

void
ll_disk_cache::reset(ll_disk_cache_config const& config)
{
    auto& cache = *this->impl_;
    std::scoped_lock<std::mutex> lock(cache.mutex);

    shut_down(cache);
    initialize(cache, config);
}

disk_cache_info
ll_disk_cache::get_summary_info()
{
    auto& cache = *this->impl_;
    std::scoped_lock<std::mutex> lock(cache.mutex);

    disk_cache_info info;
    info.directory = cache.dir.string();
    info.size_limit = cache.size_limit;
    info.ac_entry_count = get_ac_entry_count(cache);
    info.cas_entry_count = get_cas_entry_count(cache);
    info.total_size = get_total_cas_size(cache);
    return info;
}

std::vector<ll_disk_cache_cas_entry>
ll_disk_cache::get_cas_entry_list()
{
    auto& cache = *this->impl_;
    std::scoped_lock<std::mutex> lock(cache.mutex);

    return cradle::get_cas_entry_list(cache);
}

void
ll_disk_cache::remove_entry(int64_t ac_id)
{
    auto& cache = *this->impl_;
    cache.logger->info("remove_entry: ac_id {}", ac_id);
    std::scoped_lock<std::mutex> lock(cache.mutex);

    remove_ac_entry_with_cas_entry(cache, ac_id);
}

void
ll_disk_cache::clear()
{
    auto& cache = *this->impl_;
    cache.logger->info("clear");
    std::scoped_lock<std::mutex> lock(cache.mutex);

    remove_all_entries(cache);
}

std::optional<ll_disk_cache_cas_entry>
ll_disk_cache::find(std::string const& ac_key)
{
    auto& cache = *this->impl_;
    std::scoped_lock<std::mutex> lock(cache.mutex);

    record_activity(cache);

    return look_up(cache, ac_key);
}

std::optional<int64_t>
ll_disk_cache::look_up_ac_id(std::string const& ac_key)
{
    auto& cache = *this->impl_;
    cache.logger->info("look_up_ac_id {}", ac_key);
    std::scoped_lock<std::mutex> lock(cache.mutex);

    record_activity(cache);

    return cradle::look_up_ac_id(cache, ac_key);
}

void
ll_disk_cache::insert(
    std::string const& ac_key,
    std::string const& digest,
    blob const& value,
    std::optional<std::size_t> original_size)
{
    auto& cache = *this->impl_;
    cache.logger->info("insert: ac_key {}, digest {}", ac_key, digest);
    std::scoped_lock<std::mutex> lock(cache.mutex);

    record_activity(cache);

    auto opt_cas_id_for_ac = look_up_cas_id(cache, ac_key);
    if (opt_cas_id_for_ac)
    {
        // The entries already exist; must be a race condition
        cache.logger->info(
            " insert: ac_key {} already there, cas_id {}",
            ac_key,
            *opt_cas_id_for_ac);
        return;
    }
    auto opt_cas_id_for_cas = look_up_cas_id_by_digest(cache, digest);
    int64_t cas_id{};
    uint64_t growth{};
    if (opt_cas_id_for_cas)
    {
        cas_id = *opt_cas_id_for_cas;
    }
    else
    {
        auto stored_original_size
            = original_size ? *original_size : value.size();
        cas_id = insert_cas_entry(cache, digest, value, stored_original_size);
        growth = value.size();
    }
    insert_ac_entry(cache, ac_key, cas_id);
    record_cache_growth(cache, growth);
}

std::optional<int64_t>
ll_disk_cache::initiate_insert(
    std::string const& ac_key, std::string const& digest)
{
    auto& cache = *this->impl_;
    cache.logger->info(
        "initiate_insert: ac_key {}, digest {}", ac_key, digest);
    std::scoped_lock<std::mutex> lock(cache.mutex);

    record_activity(cache);

    auto opt_cas_id_for_ac = look_up_cas_id(cache, ac_key);
    if (opt_cas_id_for_ac)
    {
        // The entries already exist; must be a race condition
        cache.logger->info(
            " initiate_insert: ac_key {} already there, cas_id {}",
            ac_key,
            *opt_cas_id_for_ac);
        return std::nullopt;
    }
    auto opt_cas_id_for_cas = look_up_cas_id_by_digest(cache, digest);
    if (opt_cas_id_for_cas)
    {
        // A suitable CAS entry already exists; just create an AC entry
        // referring to it. The CAS entry could be invalid; if so, someone else
        // should be writing the file and call finish_insert() when done, but
        // we cannot verify this.
        cache.logger->info(
            " initiate_insert: found CAS entry with cas_id {}",
            *opt_cas_id_for_cas);
        insert_ac_entry(cache, ac_key, *opt_cas_id_for_cas);
        return std::nullopt;
    }
    auto cas_id = initiate_cas_insert(cache, digest);
    insert_ac_entry(cache, ac_key, cas_id);
    return cas_id;
}

void
ll_disk_cache::finish_insert(
    int64_t cas_id, std::size_t size, std::size_t original_size)
{
    auto& cache = *this->impl_;
    cache.logger->info(
        "finish_insert: cas_id {}, size {}, original_size {}",
        cas_id,
        size,
        original_size);
    std::scoped_lock<std::mutex> lock(cache.mutex);

    record_activity(cache);

    finish_cas_insert(cache, cas_id, size, original_size);

    record_cache_growth(cache, size);
}

file_path
ll_disk_cache::get_path_for_digest(std::string const& digest)
{
    auto& cache = *this->impl_;
    std::scoped_lock<std::mutex> lock(cache.mutex);

    return cradle::get_path_for_digest(cache, digest);
}

void
ll_disk_cache::flush_ac_usage(bool forced)
{
    auto& cache = *this->impl_;
    // cache.logger->info("flush_ac_usage forced {}", forced);
    std::scoped_lock<std::mutex> lock(cache.mutex);

    if (forced || should_flush_ac_usage(cache))
    {
        cradle::flush_ac_usage(cache);
    }
}

} // namespace cradle
