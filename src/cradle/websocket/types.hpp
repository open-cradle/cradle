#ifndef CRADLE_WEBSOCKET_TYPES_HPP
#define CRADLE_WEBSOCKET_TYPES_HPP

#include <cradle/thinknode/types.hpp>

namespace cradle {

struct lambda_calculation;
struct hybrid_function_application;
struct hybrid_array_request;
struct hybrid_item_request;
struct hybrid_object_request;
struct hybrid_property_request;
struct hybrid_let_request;
struct hybrid_meta_request;
struct hybrid_cast_request;

api(union)
union hybrid_calculation_request
{
    std::string reference;
    dynamic value;
    lambda_calculation lambda;
    hybrid_function_application function;
    hybrid_array_request array;
    hybrid_item_request item;
    hybrid_object_request object;
    hybrid_property_request property;
    hybrid_let_request let;
    std::string variable;
    hybrid_meta_request meta;
    hybrid_cast_request cast;
};

// TODO: Maybe come up with a better name for this.
api(enum)
enum class execution_host_selection
{
    ANY,
    THINKNODE,
    LOCAL
};

api(struct)
struct hybrid_function_application
{
    std::string account;
    std::string app;
    std::string name;
    execution_host_selection host;
    omissible<cradle::integer> level;
    std::vector<hybrid_calculation_request> args;
};

// TODO
api(struct)
struct lambda_calculation
{
};

api(struct)
struct hybrid_array_request
{
    std::vector<hybrid_calculation_request> items;
    cradle::thinknode_type_info item_schema;
};

api(struct)
struct hybrid_object_request
{
    std::map<std::string, hybrid_calculation_request> properties;
    cradle::thinknode_type_info schema;
};

api(struct)
struct hybrid_item_request
{
    hybrid_calculation_request array;
    hybrid_calculation_request index;
    cradle::thinknode_type_info schema;
};

api(struct)
struct hybrid_property_request
{
    hybrid_calculation_request object;
    hybrid_calculation_request field;
    cradle::thinknode_type_info schema;
};

api(struct)
struct hybrid_meta_request
{
    hybrid_calculation_request generator;
    cradle::thinknode_type_info schema;
};

api(struct)
struct hybrid_cast_request
{
    cradle::thinknode_type_info schema;
    hybrid_calculation_request object;
};

api(struct)
struct hybrid_let_request
{
    std::map<std::string, hybrid_calculation_request> variables;
    hybrid_calculation_request in;
};

} // namespace cradle

#endif
