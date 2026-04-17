#pragma once

#define JSON_EachChild(root) JSON_Element *it = (root)->first_child; it != NULL; it = it->next_sibling
#define JSON_Each(elem) JSON_Element *it = (elem); it != NULL; it = it->next_sibling

typedef enum JSON_Token_Type
{
    JSON_Token_None,
    JSON_Token_Error,

    JSON_Token_OpenBrace,
    JSON_Token_CloseBrace,
    JSON_Token_OpenBracket,
    JSON_Token_CloseBracket,
    JSON_Token_Comma,
    JSON_Token_Colon,

    JSON_Token_Object,
    JSON_Token_Array,
    JSON_Token_Number,
    JSON_Token_String,
    JSON_Token_Boolean,
    JSON_Token_Null,

    JSON_Token_COUNT,
}
JSON_Token_Type;

typedef struct JSON_Token JSON_Token;
struct JSON_Token
{
    JSON_Token_Type type;
    String value;
};

typedef struct JSON_Parser JSON_Parser;
struct JSON_Parser
{
    String text;
    u64 at;
    b32 error;
};

typedef struct JSON_Element JSON_Element;
struct JSON_Element
{
    JSON_Token_Type type;
    String label;
    String value;

    JSON_Element *first_child;
    JSON_Element *next_sibling;
};

//
// Impl
//

function b32 json_in_bounds(String text, u64 at)
{
    return at < text.count;
}

function b32 json_is_whitespace(String text, u64 at)
{
    b32 result = false;
    if (json_in_bounds(text, at))
    {
        u8 it = text.data[at];
        result = it == ' ' || it == '\n' || it == '\t' || it == '\r' || it == '\f';
    }
    return result;
}

function b32 json_is_digit(String text, u64 at)
{
    b32 result = false;
    if (json_in_bounds(text, at))
    {
        u8 it = text.data[at];
        result = it >= '0' && it <= '9';
    }
    return result;
}

function b32 json_is_parsing(JSON_Parser *parser)
{
    return !parser->error && json_in_bounds(parser->text, parser->at);
}

function void json_error(JSON_Parser *parser, JSON_Token token, const char *message)
{
    parser->error = true;
    print("ERROR: %.*s - %s\n", LIT(token.value), message);
}

function void json_parse_keyword(String text, u64 *at, String keyword, JSON_Token_Type type, JSON_Token *result) {
    if ((text.count - *at) >= keyword.count)
    {
        String check = text;
        check.data += *at;
        check.count = keyword.count;

        if (string_equals(check, keyword))
        {
            result->type = type;
            result->value.count += keyword.count;
            *at += keyword.count;
        }
    }
}

function String json_token_type_to_string(JSON_Token_Type type)
{
    String table[] = {
        S("None"),
        S("Error"),

        S("OpenBrace"),
        S("CloseBrace"),
        S("OpenBracket"),
        S("CloseBracket"),
        S("Comma"),
        S("Colon"),

        S("Number"),
        S("String"),
        S("Boolean"),
        S("Null"),

        S("COUNT"),
    };

    String result = S("");
    if (type < count_of(table))
    {
        result = table[type];
    }
    return result;
}

function JSON_Token json_get_token(JSON_Parser *parser)
{
    JSON_Token result = {0};

    String text = parser->text;
    u64 at = parser->at;

    while (json_is_whitespace(text, at))
    {
        at += 1;
    }

    if (json_in_bounds(text, at))
    {
        result.type = JSON_Token_Error;
        result.value.count = 1;
        result.value.data = text.data + at;

        u8 val = text.data[at];
        at += 1;

        switch (val)
        {
            case '{': { result.type = JSON_Token_OpenBrace; } break;
            case '}': { result.type = JSON_Token_CloseBrace; } break;
            case '[': { result.type = JSON_Token_OpenBracket; } break;
            case ']': { result.type = JSON_Token_CloseBracket; } break;
            case ',': { result.type = JSON_Token_Comma; } break;
            case ':': { result.type = JSON_Token_Colon; } break;

            case 'n':
            {
                json_parse_keyword(text, &at, S("ull"), JSON_Token_Null, &result);
            } break;

            case 't':
            {
                json_parse_keyword(text, &at, S("rue"), JSON_Token_Boolean, &result);
            } break;

            case 'f':
            {
                json_parse_keyword(text, &at, S("alse"), JSON_Token_Boolean, &result);
            } break;

            case '"':
            {
                result.type = JSON_Token_String;

                u64 start = at;
                while (json_in_bounds(text, at) && text.data[at] != '"')
                {
                    if (json_in_bounds(text, at + 1) && text.data[at] == '\\' && text.data[at + 1] == '"')
                    {
                        at += 1;
                    }

                    at += 1;
                }

                result.value.data = text.data + start;
                result.value.count = at - start;

                if (json_in_bounds(text, at))
                {
                    at += 1;
                }
            } break;

            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            {
                u64 start = at - 1;
                result.type = JSON_Token_Number;

                // NOTE(nick): move past a leading negative sign if one exists
                if (val == '-' && json_in_bounds(text, at))
                {
                    val = text.data[at];
                    at += 1;
                }

                // NOTE(nick): if the leading digit wasn't 0, parse any digits before the decimal point
                if (val != '0')
                {
                    while (json_is_digit(text, at))
                    {
                        at += 1;
                    }
                }

                // NOTE(nick): if there is a decimal point, parse any digits after it
                if (json_in_bounds(text, at) && text.data[at] == '.')
                {
                    at += 1;
                    while (json_is_digit(text, at))
                    {
                        at += 1;
                    }
                }

                // NOTE(nick): if it's in scientific notation, parse any digits after the "e"
                if (json_in_bounds(text, at) && (text.data[at] == 'e' || text.data[at] == 'E'))
                {
                    at += 1;

                    if (json_in_bounds(text, at) && (text.data[at] == '+' || text.data[at] == '-'))
                    {
                        at += 1;
                    }

                    while (json_is_digit(text, at))
                    {
                        at += 1;
                    }
                }

                result.value.count = at - start;
            } break;

            default:
            {
            } break;
        }
    }

    parser->at = at;

    return result;
}

function JSON_Element *json_parse_list(Arena *arena, JSON_Parser *parser, JSON_Token start_token, JSON_Token_Type end_type, b32 has_labels);

function JSON_Element *json_parse_element(Arena *arena, JSON_Parser *parser, String label, JSON_Token token)
{
    b32 valid = true;
    JSON_Element *child = NULL;
    JSON_Token_Type type = JSON_Token_None;

    if (token.type == JSON_Token_OpenBrace)
    {
        child = json_parse_list(arena, parser, token, JSON_Token_CloseBrace, true);
        type = JSON_Token_Object;
    }
    else if (token.type == JSON_Token_OpenBracket)
    {
        child = json_parse_list(arena, parser, token, JSON_Token_CloseBracket, false);
        type = JSON_Token_Array;
    }
    else if ((
            token.type == JSON_Token_String ||
            token.type == JSON_Token_Number ||
            token.type == JSON_Token_Boolean ||
            token.type == JSON_Token_Null
    ))
    {
        type = token.type;
    }
    else
    {
        valid = false;
    }

    JSON_Element *result = NULL;
    if (valid)
    {
        result = PushStruct(arena, JSON_Element);
        result->label = label;
        result->type = type;
        result->value = token.value;
        result->first_child = child;
        result->next_sibling = NULL;
    }

    return result;
}

function JSON_Element *json_parse_list(Arena *arena, JSON_Parser *parser, JSON_Token start_token, JSON_Token_Type end_type, b32 has_labels)
{
    JSON_Element *first_element = NULL;
    JSON_Element *last_element = NULL;

    while (json_is_parsing(parser))
    {
        String label = {0};
        JSON_Token token = json_get_token(parser);
        if (has_labels)
        {
            if (token.type == JSON_Token_String)
            {
                label = token.value;

                JSON_Token colon = json_get_token(parser);
                if (colon.type == JSON_Token_Colon)
                {
                    token = json_get_token(parser);
                }
                else
                {
                    json_error(parser, colon, "Expected colon after field name");
                }
            }
            else if (token.type != end_type)
            {
                json_error(parser, token, "Unexpected token in JSON");
            }
        }

        JSON_Element *element = json_parse_element(arena, parser, label, token);
        if (element)
        {
            if (last_element)
            {
                last_element = last_element->next_sibling = element;
            }
            else
            {
                first_element = last_element = element;
            }
        }
        else if (token.type == end_type)
        {
            break;
        }
        else
        {
            json_error(parser, token, "Unexpected token in JSON");
        }

        JSON_Token comma = json_get_token(parser);
        if (comma.type == end_type)
        {
            break;
        }
        else if (comma.type != JSON_Token_Comma)
        {
            json_error(parser, comma, "Unexpected token in JSON (expected comma)");
        }
    }

    return first_element;
}

//
// User API
//

function JSON_Element *json_parse(Arena *arena, String text)
{
    JSON_Parser parser = {0};
    parser.text = text;

    JSON_Element *result = json_parse_element(arena, &parser, S(""), json_get_token(&parser));

    while (json_is_whitespace(parser.text, parser.at))
    {
        parser.at += 1;
    }

    if (json_is_parsing(&parser))
    {
        json_error(&parser, json_get_token(&parser), "Unexpected token in JSON (expected EOF)");
    }
    
    return result;
}

function JSON_Element *json_find(JSON_Element *object, String child_name)
{
    JSON_Element *result = NULL;
    if (object)
    {
        for (JSON_Element *search = object->first_child; search != NULL; search = search->next_sibling)
        {
            if (string_equals(search->label, child_name))
            {
                result = search;
                break;
            }
        }
    }
    return result;
}

function u64 json_child_count(JSON_Element *element)
{
    u64 result = 0;
    for (JSON_EachChild(element))
    {
        result += 1;
    }
    return result;
}

function b32 json_is_object(JSON_Element *element)
{
    return element->type == JSON_Token_Object;
}

function b32 json_is_array(JSON_Element *element)
{
    return element->type == JSON_Token_Array;
}

function b32 json_is_bool(JSON_Element *element)
{
    return element->type == JSON_Token_Boolean;
}

function b32 json_is_number(JSON_Element *element)
{
    return element->type == JSON_Token_Number;
}

function b32 json_is_string(JSON_Element *element)
{
    return element->type == JSON_Token_String;
}

function b32 json_is_null(JSON_Element *element)
{
    return element->type == JSON_Token_Null;
}


function b32 json_to_b32(JSON_Element *element)
{
    b32 result = false;
    if (element)
    {
        if (element->type == JSON_Token_Boolean && element->value.data[0] == 't')
        {
            result = true;
        }
    }
    return result;
}

function f64 json_to_f64(JSON_Element *element)
{
    f64 result = 0.0;
    if (element && element->type == JSON_Token_Number)
    {
        String text = element->value;
        u64 at = 0;

        // NOTE(nick): sign
        f64 sign = 1.0;
        if (json_in_bounds(text, at) && text.data[at] == '-')
        {
            sign = -1.0;
            at += 1;
        }

        // NOTE(nick): int
        f64 number = 0.0;
        while (json_in_bounds(text, at))
        {
            u8 part = text.data[at] - (u8)'0';
            if (part < 10)
            {
                number = 10.0*number + (f64)part;
                at += 1;
            }
            else
            {
                break;
            }
        }

        // NOTE(nick): decimal
        if (json_in_bounds(text, at) && text.data[at] == '.')
        {
            at += 1;
            f64 c = 1.0 / 10.0;
            while (json_in_bounds(text, at))
            {
                u8 part = text.data[at] - (u8)'0';
                if (part < 10)
                {
                    number = number + c * (f64)part;
                    c *= 1.0 / 10.0;
                    at += 1;
                }
                else
                {
                    break;
                }
            }
        }

        // NOTE(nick): scientific notation
        if (json_in_bounds(text, at) && (text.data[at] == 'e' || text.data[at] == 'E'))
        {
            at += 1;
            if (json_in_bounds(text, at) && text.data[at] == '+')
            {
                at += 1;
            }

            // NOTE(nick): exponent sign
            f64 exponent_sign = 1.0;
            if (json_in_bounds(text, at) && text.data[at] == '-')
            {
                exponent_sign = -1.0;
                at += 1;
            }

            // NOTE(nick): exponent int
            f64 exponent = 0.0;
            while (json_in_bounds(text, at))
            {
                u8 part = text.data[at] - (u8)'0';
                if (part < 10)
                {
                    exponent = 10.0*exponent + (f64)part;
                    at += 1;
                }
                else
                {
                    break;
                }
            }

            exponent = exponent_sign * exponent;
            number *= pow(10.0, exponent);
        }

        result = sign * number;
    }
    return result;
}

function i64 json_to_i64(JSON_Element *element)
{
    return (i64)json_to_f64(element);
}

function String json_to_string(JSON_Element *element)
{
    String result = {0};
    if (element)
    {
        if (element->type == JSON_Token_String)
        {
            result = element->value;
        }
    }
    return result;
}

function void json_dump_internal(JSON_Element *element, i64 depth)
{
    for (i64 i = 0; i < depth; i += 1) print("  ");
    print("%c\n", json_is_array(element) ? '[' : '{');

    depth += 1;
    for (JSON_EachChild(element))
    {
        for (i64 i = 0; i < depth; i += 1) print("  ");
        if (it->label.count > 0)
        {
            print("\"%.*s\": ", LIT(it->label));
        }

        if (it->type == JSON_Token_Array || it->type == JSON_Token_Object)
        {
            print("\n");
            json_dump_internal(it, depth);
        }
        else
        {
            if (it->type == JSON_Token_String)
            {
                print("\"%.*s\"", LIT(it->value));
            }
            else
            {
                print("%.*s", LIT(it->value));
            }
        }

        print(",\n");
    }

    depth -= 1;
    for (i64 i = 0; i < depth; i += 1) print("  ");
    print("%c\n", json_is_array(element) ? ']' : '}');
}

function void json_dump(JSON_Element *element)
{
    json_dump_internal(element, 0);
}