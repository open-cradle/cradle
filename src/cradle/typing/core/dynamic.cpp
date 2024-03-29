#include <cradle/typing/core/dynamic.h>

#include <algorithm>

#include <cradle/inner/utilities/functional.h>
#include <cradle/typing/core.h>
#include <cradle/typing/encodings/yaml.h>

namespace cradle {

std::ostream&
operator<<(std::ostream& s, value_type t)
{
    switch (t)
    {
        case value_type::NIL:
            s << "nil";
            break;
        case value_type::BOOLEAN:
            s << "boolean";
            break;
        case value_type::INTEGER:
            s << "integer";
            break;
        case value_type::FLOAT:
            s << "float";
            break;
        case value_type::STRING:
            s << "string";
            break;
        case value_type::BLOB:
            s << "blob";
            break;
        case value_type::DATETIME:
            s << "datetime";
            break;
        case value_type::ARRAY:
            s << "array";
            break;
        case value_type::MAP:
            s << "map";
            break;
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("value_type") << enum_value_info(int(t)));
    }
    return s;
}

void
check_type(value_type expected, value_type actual)
{
    if (expected != actual)
    {
        CRADLE_THROW(
            type_mismatch() << expected_value_type_info(expected)
                            << actual_value_type_info(actual));
    }
}

dynamic::dynamic(std::initializer_list<dynamic> list)
{
    // If this is a list of arrays, all of which are length two and have
    // strings as their first elements, treat it as a map.
    if (std::all_of(list.begin(), list.end(), [](dynamic const& v) {
            return v.type() == value_type::ARRAY
                   && cast<dynamic_array>(v).size() == 2
                   && cast<dynamic_array>(v)[0].type() == value_type::STRING;
        }))
    {
        dynamic_map map;
        for (auto v : list)
        {
            auto const& array = cast<dynamic_array>(v);
            map[array[0]] = array[1];
        }
        *this = dynamic(std::move(map));
    }
    else
    {
        *this = dynamic(dynamic_array(list));
    }
}

void
swap(dynamic& a, dynamic& b)
{
    using std::swap;
    swap(a.storage_, b.storage_);
}

std::ostream&
operator<<(std::ostream& os, dynamic const& v)
{
    os << value_to_diagnostic_yaml(v);
    return os;
}

std::ostream&
operator<<(std::ostream& os, std::list<dynamic> const& v)
{
    os << dynamic(std::vector<dynamic>{std::begin(v), std::end(v)});
    return os;
}

size_t
deep_sizeof(dynamic const& v)
{
    return sizeof(dynamic) + apply_to_dynamic(CRADLE_LAMBDIFY(deep_sizeof), v);
}

size_t
hash_value(dynamic const& x)
{
    return apply_to_dynamic(CRADLE_LAMBDIFY(invoke_hash), x);
}

// COMPARISON OPERATORS

bool
operator==(dynamic const& a, dynamic const& b)
{
    if (a.type() != b.type())
        return false;
    return apply_to_dynamic_pair(
        [](auto const& x, auto const& y) { return x == y; }, a, b);
}
bool
operator!=(dynamic const& a, dynamic const& b)
{
    return !(a == b);
}

bool
operator<(dynamic const& a, dynamic const& b)
{
    if (a.type() != b.type())
        return a.type() < b.type();
    return apply_to_dynamic_pair(
        [](auto const& x, auto const& y) { return x < y; }, a, b);
}
bool
operator<=(dynamic const& a, dynamic const& b)
{
    return !(b < a);
}
bool
operator>(dynamic const& a, dynamic const& b)
{
    return b < a;
}
bool
operator>=(dynamic const& a, dynamic const& b)
{
    return !(a < b);
}

dynamic const&
get_field(dynamic_map const& r, string const& field)
{
    dynamic const* v;
    if (!get_field(&v, r, field))
    {
        CRADLE_THROW(missing_field() << field_name_info(field));
    }
    return *v;
}

dynamic&
get_field(dynamic_map& r, string const& field)
{
    dynamic* v;
    if (!get_field(&v, r, field))
    {
        CRADLE_THROW(missing_field() << field_name_info(field));
    }
    return *v;
}

bool
get_field(dynamic const** v, dynamic_map const& r, string const& field)
{
    auto i = r.find(dynamic(field));
    if (i == r.end())
        return false;
    *v = &i->second;
    return true;
}

bool
get_field(dynamic** v, dynamic_map& r, string const& field)
{
    auto i = r.find(dynamic(field));
    if (i == r.end())
        return false;
    *v = &i->second;
    return true;
}

dynamic const&
get_union_tag(dynamic_map const& map)
{
    if (map.size() != 1)
    {
        CRADLE_THROW(multifield_union());
    }
    return map.begin()->first;
}

void
type_info_query<dynamic>::get(api_type_info* info)
{
    *info = make_api_type_info_with_dynamic_type(api_dynamic_type());
}

void
add_dynamic_path_element(boost::exception& e, dynamic path_element)
{
    std::list<dynamic>* info = get_error_info<dynamic_value_path_info>(e);
    if (info)
    {
        info->push_front(std::move(path_element));
    }
    else
    {
        e << dynamic_value_path_info(
            std::list<dynamic>({std::move(path_element)}));
    }
}

namespace detail {

cppcoro::task<bool>
value_requires_coercion(
    std::function<cppcoro::task<api_type_info>(
        api_named_type_reference const& ref)> const& look_up_named_type,
    api_type_info const& type,
    dynamic const& value)
{
    auto recurse = [&look_up_named_type](
                       api_type_info const& type,
                       dynamic const& value) -> cppcoro::task<bool> {
        return value_requires_coercion(look_up_named_type, type, value);
    };

    switch (get_tag(type))
    {
        case api_type_info_tag::ARRAY_TYPE: {
            integer index = 0;
            for (auto const& item : cast<dynamic_array>(value))
            {
                auto this_index = index++;
                try
                {
                    if (co_await recurse(
                            as_array_type(type).element_schema, item))
                        co_return true;
                }
                catch (boost::exception& e)
                {
                    cradle::add_dynamic_path_element(e, this_index);
                    throw;
                }
            }
            co_return false;
        }
        case api_type_info_tag::BLOB_TYPE:
            check_type(value_type::BLOB, value.type());
            co_return false;
        case api_type_info_tag::BOOLEAN_TYPE:
            check_type(value_type::BOOLEAN, value.type());
            co_return false;
        case api_type_info_tag::DATETIME_TYPE:
            // Be forgiving of clients that leave their datetimes as strings.
            if (value.type() == value_type::STRING)
            {
                try
                {
                    parse_ptime(cast<string>(value));
                    co_return true;
                }
                catch (...)
                {
                }
            }
            check_type(value_type::DATETIME, value.type());
            co_return false;
        case api_type_info_tag::DYNAMIC_TYPE:
            co_return false;
        case api_type_info_tag::ENUM_TYPE:
            check_type(value_type::STRING, value.type());
            if (as_enum_type(type).values.find(cast<string>(value))
                == as_enum_type(type).values.end())
            {
                CRADLE_THROW(
                    cradle::invalid_enum_string()
                    << cradle::enum_string_info(cast<string>(value)));
            }
            co_return false;
        case api_type_info_tag::FLOAT_TYPE:
            if (value.type() == value_type::INTEGER)
                co_return true;
            check_type(value_type::FLOAT, value.type());
            co_return false;
        case api_type_info_tag::INTEGER_TYPE:
            if (value.type() == value_type::FLOAT)
            {
                double d = cast<double>(value);
                integer i = boost::numeric_cast<integer>(d);
                // Check that coercion doesn't change the value.
                if (boost::numeric_cast<double>(i) == d)
                    co_return true;
            }
            check_type(value_type::INTEGER, value.type());
            co_return false;
        case api_type_info_tag::MAP_TYPE: {
            auto const& map_type = as_map_type(type);
            // This is a little hack to support the fact that JSON maps are
            // encoded as arrays and they don't get recognized as maps when
            // they're empty.
            if (value.type() == value_type::ARRAY
                && cast<dynamic_array>(value).empty())
            {
                co_return true;
            }
            for (auto const& key_value : cast<dynamic_map>(value))
            {
                try
                {
                    if (co_await recurse(map_type.key_schema, key_value.first)
                        || co_await recurse(
                            map_type.value_schema, key_value.second))
                    {
                        co_return true;
                    }
                }
                catch (boost::exception& e)
                {
                    cradle::add_dynamic_path_element(e, key_value.first);
                    throw;
                }
            }
            co_return false;
        }
        case api_type_info_tag::NAMED_TYPE:
            co_return co_await recurse(
                co_await look_up_named_type(as_named_type(type)), value);
        case api_type_info_tag::NIL_TYPE:
        default:
            check_type(value_type::NIL, value.type());
            co_return false;
        case api_type_info_tag::OPTIONAL_TYPE: {
            auto const& map = cast<dynamic_map>(value);
            auto const& tag = cast<string>(cradle::get_union_tag(map));
            if (tag == "some")
            {
                try
                {
                    co_return co_await recurse(
                        as_optional_type(type), get_field(map, "some"));
                }
                catch (boost::exception& e)
                {
                    cradle::add_dynamic_path_element(e, "some");
                    throw;
                }
            }
            else if (tag == "none")
            {
                check_type(value_type::NIL, get_field(map, "none").type());
                co_return false;
            }
            else
            {
                CRADLE_THROW(
                    invalid_optional_type() << optional_type_tag_info(tag));
            }
        }
        case api_type_info_tag::REFERENCE_TYPE:
            check_type(value_type::STRING, value.type());
            co_return false;
        case api_type_info_tag::STRING_TYPE:
            check_type(value_type::STRING, value.type());
            co_return false;
        case api_type_info_tag::STRUCTURE_TYPE: {
            auto const& structure_type = as_structure_type(type);
            auto const& map = cast<dynamic_map>(value);
            for (auto const& key_value : structure_type.fields)
            {
                auto const& field_name = key_value.first;
                auto const& field_info = key_value.second;
                dynamic const* field_value;
                bool field_present
                    = get_field(&field_value, map, key_value.first);
                if (field_present)
                {
                    try
                    {
                        if (co_await recurse(field_info.schema, *field_value))
                            co_return true;
                    }
                    catch (boost::exception& e)
                    {
                        cradle::add_dynamic_path_element(e, field_name);
                        throw;
                    }
                }
                else if (!field_info.omissible || !*field_info.omissible)
                {
                    CRADLE_THROW(
                        missing_field() << field_name_info(field_name));
                }
            }
            co_return false;
        }
        case api_type_info_tag::UNION_TYPE: {
            auto const& union_type = as_union_type(type);
            auto const& map = cast<dynamic_map>(value);
            auto const& tag = cast<string>(cradle::get_union_tag(map));
            for (auto const& key_value : union_type.members)
            {
                auto const& member_name = key_value.first;
                auto const& member_info = key_value.second;
                if (tag == member_name)
                {
                    try
                    {
                        co_return co_await recurse(
                            member_info.schema, get_field(map, member_name));
                    }
                    catch (boost::exception& e)
                    {
                        cradle::add_dynamic_path_element(e, member_name);
                        throw;
                    }
                }
            }
            CRADLE_THROW(
                cradle::invalid_enum_string() <<
                // This should technically include enum_id_info.
                cradle::enum_string_info(tag));
        }
    }
}

} // namespace detail

cppcoro::task<void>
coerce_value_impl(
    std::function<cppcoro::task<api_type_info>(
        api_named_type_reference const& ref)> const& look_up_named_type,
    api_type_info const& type,
    dynamic& value)
{
    auto recurse = [&look_up_named_type](
                       api_type_info const& type,
                       dynamic& value) -> cppcoro::task<void> {
        return coerce_value_impl(look_up_named_type, type, value);
    };

    switch (get_tag(type))
    {
        case api_type_info_tag::ARRAY_TYPE: {
            integer index = 0;
            for (dynamic& item : cast<dynamic_array>(value))
            {
                auto this_index = index++;
                try
                {
                    co_await recurse(as_array_type(type).element_schema, item);
                }
                catch (boost::exception& e)
                {
                    cradle::add_dynamic_path_element(e, this_index);
                    throw;
                }
            }
            break;
        }
        case api_type_info_tag::BLOB_TYPE:
            check_type(value_type::BLOB, value.type());
            break;
        case api_type_info_tag::BOOLEAN_TYPE:
            check_type(value_type::BOOLEAN, value.type());
            break;
        case api_type_info_tag::DATETIME_TYPE:
            // Be forgiving of clients that leave their datetimes as strings.
            if (value.type() == value_type::STRING)
            {
                try
                {
                    value = dynamic(parse_ptime(cast<string>(value)));
                    break;
                }
                catch (...)
                {
                }
            }
            check_type(value_type::DATETIME, value.type());
            break;
        case api_type_info_tag::DYNAMIC_TYPE:
            break;
        case api_type_info_tag::ENUM_TYPE:
            check_type(value_type::STRING, value.type());
            if (as_enum_type(type).values.find(cast<string>(value))
                == as_enum_type(type).values.end())
            {
                CRADLE_THROW(
                    cradle::invalid_enum_string()
                    << cradle::enum_string_info(cast<string>(value)));
            }
            break;
        case api_type_info_tag::FLOAT_TYPE:
            if (value.type() == value_type::INTEGER)
            {
                value = boost::numeric_cast<double>(cast<integer>(value));
                break;
            }
            check_type(value_type::FLOAT, value.type());
            break;
        case api_type_info_tag::INTEGER_TYPE:
            if (value.type() == value_type::FLOAT)
            {
                double d = cast<double>(value);
                integer i = boost::numeric_cast<integer>(d);
                // Check that coercion doesn't change the value.
                if (boost::numeric_cast<double>(i) == d)
                {
                    value = dynamic(i);
                    break;
                }
            }
            check_type(value_type::INTEGER, value.type());
            break;
        case api_type_info_tag::MAP_TYPE: {
            auto const& map_type = as_map_type(type);
            // This is a little hack to support the fact that JSON maps are
            // encoded as arrays and they don't get recognized as maps when
            // they're empty.
            if (value.type() == value_type::ARRAY
                && cast<dynamic_array>(value).empty())
            {
                value = dynamic_map();
                break;
            }
            // Since we can't mutate the keys in the map, first check to see if
            // that's necessary.
            bool key_coercion_required = false;
            for (auto const& key_value : cast<dynamic_map>(value))
            {
                if (co_await detail::value_requires_coercion(
                        look_up_named_type,
                        map_type.key_schema,
                        key_value.first))
                {
                    key_coercion_required = true;
                    break;
                }
            }
            // If the keys need to be coerced, just create a new map.
            if (key_coercion_required)
            {
                dynamic_map coerced;
                for (auto& key_value : cast<dynamic_map>(value))
                {
                    try
                    {
                        dynamic k = key_value.first;
                        dynamic v = std::move(key_value.second);
                        co_await recurse(map_type.key_schema, k);
                        co_await recurse(map_type.value_schema, v);
                        coerced[k] = v;
                    }
                    catch (boost::exception& e)
                    {
                        cradle::add_dynamic_path_element(e, key_value.first);
                        throw;
                    }
                }
                value = coerced;
            }
            // Otherwise, coerce the values within the original map.
            else
            {
                for (auto& key_value : cast<dynamic_map>(value))
                {
                    try
                    {
                        co_await recurse(
                            map_type.value_schema, key_value.second);
                    }
                    catch (boost::exception& e)
                    {
                        cradle::add_dynamic_path_element(e, key_value.first);
                        throw;
                    }
                }
            }
            break;
        }
        case api_type_info_tag::NAMED_TYPE: {
            auto resolved_type
                = co_await look_up_named_type(as_named_type(type));
            co_await recurse(resolved_type, value);
            break;
        }
        case api_type_info_tag::NIL_TYPE:
        default:
            check_type(value_type::NIL, value.type());
            break;
        case api_type_info_tag::OPTIONAL_TYPE: {
            auto& map = cast<dynamic_map>(value);
            auto const& tag = cast<string>(cradle::get_union_tag(map));
            if (tag == "some")
            {
                try
                {
                    co_await recurse(
                        as_optional_type(type), get_field(map, "some"));
                }
                catch (boost::exception& e)
                {
                    cradle::add_dynamic_path_element(e, "some");
                    throw;
                }
            }
            else if (tag == "none")
            {
                check_type(value_type::NIL, get_field(map, "none").type());
            }
            else
            {
                CRADLE_THROW(
                    invalid_optional_type() << optional_type_tag_info(tag));
            }
            break;
        }
        case api_type_info_tag::REFERENCE_TYPE:
            check_type(value_type::STRING, value.type());
            break;
        case api_type_info_tag::STRING_TYPE:
            check_type(value_type::STRING, value.type());
            break;
        case api_type_info_tag::STRUCTURE_TYPE: {
            auto const& structure_type = as_structure_type(type);
            auto& map = cast<dynamic_map>(value);
            for (auto const& key_value : structure_type.fields)
            {
                auto const& field_name = key_value.first;
                auto const& field_info = key_value.second;
                dynamic* field_value;
                bool field_present
                    = get_field(&field_value, map, key_value.first);
                if (field_present)
                {
                    try
                    {
                        co_await recurse(field_info.schema, *field_value);
                    }
                    catch (boost::exception& e)
                    {
                        cradle::add_dynamic_path_element(e, field_name);
                        throw;
                    }
                }
                else if (!field_info.omissible || !*field_info.omissible)
                {
                    CRADLE_THROW(
                        missing_field() << field_name_info(field_name));
                }
            }
            break;
        }
        case api_type_info_tag::UNION_TYPE: {
            auto const& union_type = as_union_type(type);
            auto& map = cast<dynamic_map>(value);
            auto const& tag = cast<string>(cradle::get_union_tag(map));
            for (auto const& key_value : union_type.members)
            {
                auto const& member_name = key_value.first;
                auto const& member_info = key_value.second;
                if (tag == member_name)
                {
                    try
                    {
                        co_await recurse(
                            member_info.schema, get_field(map, member_name));
                        co_return;
                    }
                    catch (boost::exception& e)
                    {
                        cradle::add_dynamic_path_element(e, member_name);
                        throw;
                    }
                }
            }
            CRADLE_THROW(
                cradle::invalid_enum_string() <<
                // This should technically include enum_id_info.
                cradle::enum_string_info(tag));
        }
    }
}

cppcoro::task<dynamic>
coerce_value(
    std::function<cppcoro::task<api_type_info>(
        api_named_type_reference const& ref)> const& look_up_named_type,
    api_type_info type,
    dynamic value)
{
    co_await coerce_value_impl(look_up_named_type, type, value);
    co_return value;
}

} // namespace cradle
