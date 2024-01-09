#include <cradle/typing/encodings/yaml.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <yaml-cpp/yaml.h>
#pragma GCC diagnostic pop
#else
#include <yaml-cpp/yaml.h>
#endif

#include <cradle/inner/core/type_interfaces.h>
#include <cradle/inner/encodings/base64.h>
#include <cradle/inner/utilities/text.h>

namespace cradle {

// YAML I/O

static ptime
parse_time(std::string const& s)
{
    namespace bt = boost::posix_time;
    std::istringstream is(s);
    is.imbue(std::locale(
        std::cout.getloc(), new bt::time_input_facet("%Y-%m-%dT%H:%M:%s")));
    ptime t;
    is >> t;
    char z;
    is.get(z);
    if (t != ptime() && z == 'Z'
        && is.peek() == std::istringstream::traits_type::eof())
    {
        return t;
    }
    CRADLE_THROW(
        parsing_error() << expected_format_info("datetime")
                        << parsed_text_info(s));
}

static bool
safe_isdigit(char ch)
{
    return std::isdigit(static_cast<unsigned char>(ch));
}

// Read a YAML value into a CRADLE dynamic.
static dynamic
read_yaml_value(YAML::Node const& yaml)
{
    switch (yaml.Type())
    {
        case YAML::NodeType::Null:
        default: // to avoid warnings
            return dynamic(nil);
        case YAML::NodeType::Scalar: {
            // This case captures strings, booleans, integers, and doubles.
            // First, check to see if the value was explicitly quoted.
            if (yaml.Tag() == "!")
            {
                // Times are also encoded as quoted strings, so this checks to
                // see if the string parses as a time. If so, it just assumes
                // it's actually a time.
                auto s = yaml.as<string>();
                // First check if it looks anything like a time string.
                if (s.length() > 16 && safe_isdigit(s[0]) && safe_isdigit(s[1])
                    && safe_isdigit(s[2]) && safe_isdigit(s[3]) && s[4] == '-')
                {
                    try
                    {
                        auto t = parse_time(s);
                        // Check that it can be converted back without changing
                        // its value. This could be necessary if we actually
                        // expected a string here.
                        if (to_value_string(t) == s)
                        {
                            return dynamic(t);
                        }
                    }
                    catch (...)
                    {
                    }
                }
                return dynamic(std::move(s));
            }
            else // The value wasn't quoted.
            {
                auto s = yaml.as<string>();
                // Try to interpret it as a boolean.
                if (s == "true")
                    return dynamic(true);
                if (s == "false")
                    return dynamic(false);
                // Try to interpret it as a number.
                if (!s.compare(0, 2, "0x"))
                {
                    std::istringstream stream(s.substr(2));
                    integer i;
                    stream >> std::hex >> i;
                    if (!stream.fail() && stream.tellg() == std::streampos(-1))
                    {
                        return dynamic(i);
                    }
                }
                if (!s.compare(0, 2, "0o"))
                {
                    std::istringstream stream(s.substr(2));
                    integer i;
                    stream >> std::oct >> i;
                    if (!stream.fail() && stream.tellg() == std::streampos(-1))
                    {
                        return dynamic(i);
                    }
                }
                {
                    integer i;
                    if (boost::conversion::try_lexical_convert(s, i))
                    {
                        return dynamic(i);
                    }
                }
                {
                    double d;
                    if (boost::conversion::try_lexical_convert(s, d))
                    {
                        return dynamic(d);
                    }
                }
                // If all else fails, it must just be a string.
                return dynamic(std::move(s));
            }
        }
        case YAML::NodeType::Sequence: {
            dynamic_array array;
            array.reserve(yaml.size());
            for (auto const& i : yaml)
            {
                array.push_back(read_yaml_value(i));
            }
            return dynamic(std::move(array));
        }
        case YAML::NodeType::Map: {
            // An object is analogous to a map, but blobs are also encoded as
            // YAML objects, so we have to check here if it's actually one of
            // those.
            auto type = yaml["type"];
            if (type && type.IsScalar()
                && type.as<string>() == "base64-encoded-blob")
            {
                auto yaml_blob = yaml["blob"];
                if (yaml_blob && yaml_blob.IsScalar())
                {
                    string encoded = yaml_blob.as<string>();
                    size_t max_decoded_size
                        = get_base64_decoded_length(encoded.length());
                    byte_vector decoded(max_decoded_size);
                    std::size_t decoded_size;
                    base64_decode(
                        decoded.data(),
                        &decoded_size,
                        encoded.c_str(),
                        encoded.length(),
                        get_mime_base64_character_set());
                    return dynamic(
                        make_blob(std::move(decoded), decoded_size));
                }
                else
                {
                    // This was supposed to be a blob, but it's not.
                    YAML::Emitter out;
                    out << yaml;
                    CRADLE_THROW(
                        parsing_error()
                        << expected_format_info("base64-encoded-blob")
                        << parsed_text_info(string(out.c_str(), out.size()))
                        << parsing_error_info(
                               "object tagged as blob but missing data"));
                }
            }
            else
            {
                // Otherwise, interpret it as a map.
                dynamic_map map;
                for (YAML::Node::const_iterator i = yaml.begin();
                     i != yaml.end();
                     ++i)
                {
                    map[read_yaml_value(i->first)]
                        = read_yaml_value(i->second);
                }
                return dynamic(std::move(map));
            }
        }
    }
}

dynamic
parse_yaml_value(char const* yaml, size_t length)
{
    YAML::Node parsed_yaml;
    try
    {
        parsed_yaml = YAML::Load(string(yaml, yaml + length));
    }
    catch (std::exception& e)
    {
        CRADLE_THROW(
            parsing_error() << expected_format_info("YAML")
                            << parsed_text_info(string(yaml, yaml + length))
                            << parsing_error_info(e.what()));
    }
    return read_yaml_value(parsed_yaml);
}

static void
emit_yaml_value(YAML::Emitter& out, dynamic const& v)
{
    switch (v.type())
    {
        case value_type::NIL:
        default: // to avoid warnings
            out << YAML::Node();
            break;
        case value_type::BOOLEAN:
            out << cast<bool>(v);
            break;
        case value_type::INTEGER:
            out << cast<integer>(v);
            break;
        case value_type::FLOAT:
            out << cast<double>(v);
            break;
        case value_type::STRING: {
            if (read_yaml_value(YAML::Node(cast<string>(v))).type()
                != value_type::STRING)
            {
                // This happens to be a string that looks like some other
                // scalar type, so it should be explicitly quoted.
                out << YAML::DoubleQuoted << cast<string>(v);
            }
            else
            {
                out << cast<string>(v);
            }
            break;
        }
        case value_type::BLOB: {
            blob const& x = cast<blob>(v);
            YAML::Node yaml;
            yaml["type"] = "base64-encoded-blob";
            yaml["blob"] = base64_encode(
                reinterpret_cast<uint8_t const*>(x.data()),
                x.size(),
                get_mime_base64_character_set());
            out << yaml;
            break;
        }
        case value_type::DATETIME:
            out << YAML::DoubleQuoted
                << to_value_string(cast<boost::posix_time::ptime>(v));
            break;
        case value_type::ARRAY: {
            out << YAML::BeginSeq;
            for (auto const& i : cast<dynamic_array>(v))
            {
                emit_yaml_value(out, i);
            }
            out << YAML::EndSeq;
            break;
        }
        case value_type::MAP: {
            dynamic_map const& x = cast<dynamic_map>(v);
            out << YAML::BeginMap;
            for (auto const& i : x)
            {
                emit_yaml_value(out << YAML::Key, i.first);
                emit_yaml_value(out << YAML::Value, i.second);
            }
            out << YAML::EndMap;
            break;
        }
    }
}

string
value_to_yaml(dynamic const& v)
{
    YAML::Emitter out;
    out << YAML::FloatPrecision(5);
    out << YAML::DoublePrecision(12);
    emit_yaml_value(out, v);
    return out.c_str();
}

static void
emit_diagnostic_yaml_value(YAML::Emitter& out, dynamic const& v)
{
    switch (v.type())
    {
        case value_type::NIL:
        default: // to avoid warnings
            out << YAML::Node();
            break;
        case value_type::BOOLEAN:
            out << cast<bool>(v);
            break;
        case value_type::INTEGER:
            out << cast<integer>(v);
            break;
        case value_type::FLOAT:
            out << cast<double>(v);
            break;
        case value_type::STRING: {
            if (read_yaml_value(YAML::Node(cast<string>(v))).type()
                != value_type::STRING)
            {
                // This happens to be a string that looks like some other
                // scalar type, so it should be explicitly quoted.
                out << YAML::DoubleQuoted << cast<string>(v);
            }
            else
            {
                out << cast<string>(v);
            }
            break;
        }
        case value_type::BLOB: {
            std::ostringstream os;
            os << cast<blob>(v);
            out << os.str();
            break;
        }
        case value_type::DATETIME:
            out << YAML::DoubleQuoted
                << to_value_string(cast<boost::posix_time::ptime>(v));
            break;
        case value_type::ARRAY: {
            dynamic_array const& array = cast<dynamic_array>(v);
            if (array.size() < 64)
            {
                out << YAML::BeginSeq;
                for (auto const& i : array)
                {
                    emit_diagnostic_yaml_value(out, i);
                }
                out << YAML::EndSeq;
            }
            else
            {
                out << "<array - size: " + lexical_cast<string>(array.size())
                           + ">";
            }
            break;
        }
        case value_type::MAP: {
            dynamic_map const& x = cast<dynamic_map>(v);
            if (x.size() < 64)
            {
                out << YAML::BeginMap;
                for (auto const& i : x)
                {
                    emit_diagnostic_yaml_value(out << YAML::Key, i.first);
                    emit_diagnostic_yaml_value(out << YAML::Value, i.second);
                }
                out << YAML::EndMap;
            }
            else
            {
                out << "<map - size: " + lexical_cast<string>(x.size()) + ">";
            }
            break;
        }
    }
}

string
value_to_diagnostic_yaml(dynamic const& v)
{
    YAML::Emitter out;
    out << YAML::FloatPrecision(5);
    out << YAML::DoublePrecision(12);
    emit_diagnostic_yaml_value(out, v);
    return out.c_str();
}

blob
value_to_yaml_blob(dynamic const& v)
{
    return make_blob(value_to_yaml(v));
}

blob
value_to_diagnostic_yaml_blob(dynamic const& v)
{
    return make_blob(value_to_diagnostic_yaml(v));
}

} // namespace cradle
