#include "no-rust.h"
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
struct Token
{
    std::string text;
    size_t line, column;
};
std::vector<Token> lex(const std::string& s)
{
    std::vector<Token> out;
    size_t i = 0, line = 1, col = 1;
    auto step = [&]() {
        if (s[i++] == '\n')
        {
            ++line;
            col = 1;
        }
        else
        {
            ++col;
        }
    };
    auto error = [&](const char* msg) {
        throw std::runtime_error(std::to_string(line) + ":" + std::to_string(col) + ": " + msg);
    };
    while (i < s.size())
    {
        if (std::isspace(static_cast<unsigned char>(s[i])))
        {
            step();
            continue;
        }
        if (s.compare(i, 2, "//") == 0)
        {
            while (i < s.size() && s[i] != '\n')
            {
                step();
            }
            continue;
        }
        if (s.compare(i, 2, "/*") == 0)
        {
            step();
            step();
            int depth = 1;
            while (i < s.size() && depth)
            {
                if (s.compare(i, 2, "/*") == 0)
                {
                    step();
                    step();
                    ++depth;
                }
                else if (s.compare(i, 2, "*/") == 0)
                {
                    step();
                    step();
                    --depth;
                }
                else
                {
                    step();
                }
            }
            if (depth)
            {
                error("unterminated comment");
            }
            continue;
        }
        const size_t start = i, ln = line, column = col;
        if (std::isalpha(static_cast<unsigned char>(s[i])) || s[i] == '_')
        {
            do
            {
                step();
            } while (i < s.size() &&
                     (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_'));
        }
        else if (std::isdigit(static_cast<unsigned char>(s[i])))
        {
            do
            {
                step();
            } while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])));
            if (i + 1 < s.size() && s[i] == '.' &&
                std::isdigit(static_cast<unsigned char>(s[i + 1])))
            {
                step();
                while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])))
                {
                    step();
                }
            }
        }
        else if (s[i] == '"')
        {
            step();
            while (i < s.size() && s[i] != '"')
            {
                if (s[i] == '\\')
                {
                    step();
                    if (i == s.size())
                    {
                        error("unterminated string");
                    }
                    if (std::string("nrt\\\"0").find(s[i]) == std::string::npos)
                    {
                        error("unsupported escape");
                    }
                }
                if (s[i] == '\n' || s[i] == '\r')
                {
                    error("multiline strings unsupported");
                }
                step();
            }
            if (i == s.size())
            {
                error("unterminated string");
            }
            step();
        }
        else
        {
            bool found = false;
            for (const char* op : {"..=", "->", "=>", "::", "==", "!=", "<=", ">=", "&&", "||",
                                   "+=", "-=", "*=", "/=", ".."})
            {
                size_t n = std::string(op).size();
                if (s.compare(i, n, op) == 0)
                {
                    while (n--)
                    {
                        step();
                    }
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                if (std::string("(){}[];,:.+-*/%!=<>&").find(s[i]) == std::string::npos)
                {
                    error("unsupported character");
                }
                step();
            }
        }
        out.push_back({s.substr(start, i - start), ln, column});
    }
    out.push_back({"<eof>", line, col});
    return out;
}
class Parser
{
    struct Field
    {
        std::string name;
        std::string type;
    };
    enum class VariantKind
    {
        Unit,
        Tuple,
        Named,
    };
    struct Variant
    {
        std::string name;
        VariantKind kind = VariantKind::Unit;
        std::vector<Field> fields;
    };
    struct DataType
    {
        std::string name;
        bool isEnum = false;
        std::vector<Field> fields;
        std::vector<Variant> variants;
    };
    struct Function
    {
        std::string name;
        std::string owner;
        std::string result;
        std::string parameters;
        bool receiver = false;
        bool constReceiver = false;
        size_t body = 0;
    };
    std::vector<Token> ts;
    std::vector<DataType> dataTypes;
    std::vector<Function> functions;
    size_t p = 0, serial = 0;
    std::string returnType;
    std::string implType;
    bool hasReceiver = false;
    size_t valueDepth = 0;
    std::vector<std::string> continueActions;
    const std::string& peek() const
    {
        return ts[p].text;
    }
    [[noreturn]] void fail(const std::string& msg) const
    {
        throw std::runtime_error(std::to_string(ts[p].line) + ":" + std::to_string(ts[p].column) +
                                 ": " + msg + " (found '" + peek() + "')");
    }
    bool take(const std::string& s)
    {
        if (peek() != s)
        {
            return false;
        }
        ++p;
        return true;
    }
    void need(const std::string& s)
    {
        if (!take(s))
        {
            fail("expected '" + s + "'");
        }
    }
    std::string name()
    {
        const std::string s = peek();
        if (!(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_'))
        {
            fail("expected identifier");
        }
        ++p;
        return "rs_" + s; // Keep source names away from C++ keywords and runtime names.
    }
    std::string type()
    {
        if (take("["))
        {
            const std::string element = type();
            need(";");
            const std::string length = expr();
            need("]");
            return "std::array<" + element + ", " + length + ">";
        }
        if (take("&"))
        {
            const bool mutableReference = take("mut");
            return (mutableReference ? "" : "const ") + type() + "&";
        }
        if (take("String"))
        {
            return "std::string";
        }
        if (take("Self"))
        {
            if (implType.empty())
            {
                fail("Self requires an impl block");
            }
            return implType;
        }
        for (const std::pair<std::string, std::string>& e :
             std::vector<std::pair<std::string, std::string>>
             {
                 {"i8", "std::int8_t"},
                 {"i16", "std::int16_t"},
                 {"i32", "std::int32_t"},
                 {"i64", "std::int64_t"},
                 {"u8", "std::uint8_t"},
                 {"u16", "std::uint16_t"},
                 {"u32", "std::uint32_t"},
                 {"u64", "std::uint64_t"},
                 {"usize", "std::size_t"},
                 {"isize", "std::ptrdiff_t"},
                 {"f32", "float"},
                 {"f64", "double"},
                 {"bool", "bool"},
             })
        {
            if (take(e.first))
            {
                return e.second;
            }
        }
        if (take("("))
        {
            need(")");
            return "void";
        }
        return name();
    }

    const DataType* findType(const std::string& name) const
    {
        for (const DataType& data : dataTypes)
        {
            if (data.name == name)
            {
                return &data;
            }
        }
        return nullptr;
    }

    const Variant& findVariant(const DataType& data, const std::string& name) const
    {
        for (const Variant& variant : data.variants)
        {
            if (variant.name == name)
            {
                return variant;
            }
        }
        fail("unknown enum variant");
    }

    std::vector<Field> fields(const std::string& close)
    {
        std::vector<Field> result;
        while (!take(close))
        {
            take("pub");
            const std::string field = name();
            need(":");
            result.push_back({field, type()});
            if (!take(","))
            {
                need(close);
                break;
            }
        }
        return result;
    }

    void dataDeclaration(bool isEnum)
    {
        DataType data;
        data.name = name();
        data.isEnum = isEnum;
        need("{");
        if (!isEnum)
        {
            data.fields = fields("}");
        }
        else
        {
            while (!take("}"))
            {
                Variant variant;
                variant.name = name();
                if (take("("))
                {
                    variant.kind = VariantKind::Tuple;
                    while (!take(")"))
                    {
                        const std::string fieldName =
                            "nr_field_" + std::to_string(variant.fields.size());
                        variant.fields.push_back({fieldName, type()});
                        if (!take(","))
                        {
                            need(")");
                            break;
                        }
                    }
                }
                else if (take("{"))
                {
                    variant.kind = VariantKind::Named;
                    variant.fields = fields("}");
                }
                data.variants.push_back(variant);
                if (!take(","))
                {
                    need("}");
                    break;
                }
            }
            if (data.variants.empty())
            {
                fail("empty enums are not supported yet");
            }
        }
        dataTypes.push_back(data);
    }

    void functionDeclaration(const std::string& owner = "")
    {
        Function function;
        function.owner = owner;
        function.name = name();
        implType = owner;
        need("(");
        if (!take(")"))
        {
            if (!owner.empty() && (peek() == "&" || peek() == "self" || peek() == "mut"))
            {
                const bool reference = take("&");
                const bool mutableReceiver = take("mut");
                need("self");
                function.receiver = true;
                function.constReceiver = reference && !mutableReceiver;
                if (peek() != ")")
                {
                    need(",");
                }
            }
            while (!take(")"))
            {
                take("mut");
                const std::string parameter = name();
                need(":");
                const std::string parameterType = type();
                if (!function.parameters.empty())
                {
                    function.parameters += ", ";
                }
                function.parameters += parameterType + " " + parameter;
                if (!take(","))
                {
                    need(")");
                    break;
                }
            }
        }
        function.result = take("->") ? type() : "void";
        function.body = p;
        need("{");
        size_t depth = 1;
        while (depth != 0)
        {
            if (peek() == "<eof>")
            {
                fail("unterminated function body");
            }
            if (take("{"))
            {
                ++depth;
            }
            else if (take("}"))
            {
                --depth;
            }
            else
            {
                ++p;
            }
        }
        functions.push_back(function);
        implType.clear();
    }

    // Evaluate named initializers in source order, then arrange aggregate fields in declaration
    // order.
    std::string namedConstruction(const std::string& resultType, const std::vector<Field>& declared)
    {
        need("{");
        const std::string id = std::to_string(serial++);
        std::vector<std::string> values(declared.size());
        std::string result = "([&]() {\n";
        while (!take("}"))
        {
            const std::string field = name();
            size_t index = 0;
            while (index < declared.size() && declared[index].name != field)
            {
                ++index;
            }
            if (index == declared.size() || !values[index].empty())
            {
                fail("unknown or repeated initializer field");
            }
            const std::string value = take(":") ? expr() : field;
            values[index] = "nr_init_" + id + "_" + std::to_string(index);
            result += "auto " + values[index] + " = " + value + ";\n";
            if (!take(","))
            {
                need("}");
                break;
            }
        }
        result += "return " + resultType + "{";
        for (size_t i = 0; i < declared.size(); ++i)
        {
            if (values[i].empty())
            {
                fail("missing initializer field");
            }
            if (i != 0)
            {
                result += ", ";
            }
            result += "std::move(" + values[i] + ")";
        }
        return result + "};\n}())";
    }

    std::string arguments()
    {
        need("(");
        std::string result;
        while (!take(")"))
        {
            if (!result.empty())
            {
                result += ", ";
            }
            result += expr();
            if (!take(","))
            {
                need(")");
                break;
            }
        }
        return result;
    }
    static int prec(const std::string& op)
    {
        if (op == "||")
        {
            return 1;
        }
        if (op == "&&")
        {
            return 2;
        }
        if (op == "==" || op == "!=")
        {
            return 3;
        }
        if (op == "<" || op == ">" || op == "<=" || op == ">=")
        {
            return 4;
        }
        if (op == "+" || op == "-")
        {
            return 5;
        }
        if (op == "*" || op == "/" || op == "%")
        {
            return 6;
        }
        return 0;
    }
    std::string expr(int min = 1, bool allowConstruction = true)
    {
        std::string left;
        if (take("if"))
        {
            ++valueDepth;
            left = "([&]() {\n" + conditional(true) + "}())";
            --valueDepth;
        }
        else if (take("["))
        {
            if (peek() == "]")
            {
                fail("empty array literals need an element type; use [value; 0] for now");
            }
            const std::string first = expr();
            if (take(";"))
            {
                const std::string count = expr();
                need("]");
                left = "nr_repeat<" + count + ">(" + first + ")";
            }
            else
            {
                left = "std::array{" + first;
                while (take(",") && peek() != "]")
                {
                    left += ", " + expr();
                }
                need("]");
                left += "}";
            }
        }
        else if (take("match"))
        {
            left = match(true);
        }
        else if (take("&"))
        {
            take("mut");
            left = "(" + expr(7, allowConstruction) + ")";
        }
        else if (take("*"))
        {
            // Rust references use C++ references, so dereferencing needs no pointer operation.
            left = "(" + expr(7, allowConstruction) + ")";
        }
        else if (peek() == "!" || peek() == "-")
        {
            const std::string op = peek();
            ++p;
            left = "(" + op + expr(7, allowConstruction) + ")";
        }
        else if (take("("))
        {
            left = "(" + expr() + ")";
            need(")");
        }
        else if (peek() == "true" || peek() == "false" ||
                 std::isdigit(static_cast<unsigned char>(peek()[0])))
        {
            left = peek();
            ++p;
        }
        else if (peek()[0] == '"')
        {
            left = "std::string(" + peek() + ")";
            ++p;
        }
        else
        {
            if (take("self"))
            {
                if (!hasReceiver)
                {
                    fail("self requires a method receiver");
                }
                left = "(*this)";
            }
            else if (take("Self"))
            {
                if (implType.empty())
                {
                    fail("Self requires an impl block");
                }
                left = implType;
            }
            else
            {
                left = name();
            }
            const DataType* data = findType(left);
            if (take("::"))
            {
                const std::string member = name();
                if (data != nullptr && data->isEnum)
                {
                    // Associated methods share this syntax with variant constructors.
                    const Variant* variant = nullptr;
                    for (const Variant& candidate : data->variants)
                    {
                        if (candidate.name == member)
                        {
                            variant = &candidate;
                            break;
                        }
                    }
                    if (variant != nullptr)
                    {
                        const std::string caseType = left + "::nr_case_" + member;
                        std::string payload;
                        if (variant->kind == VariantKind::Named)
                        {
                            payload = namedConstruction(caseType, variant->fields);
                        }
                        else if (variant->kind == VariantKind::Tuple)
                        {
                            payload = caseType + "{" + arguments() + "}";
                        }
                        else
                        {
                            payload = caseType + "{}";
                        }
                        left += "{" + payload + "}";
                    }
                    else
                    {
                        left += "::" + member;
                    }
                }
                else if (left == "rs_String" && member == "rs_from")
                {
                    left = "std::string";
                }
                else
                {
                    left += "::" + member;
                }
            }
            else if (allowConstruction && data != nullptr && !data->isEnum && peek() == "{")
            {
                left = namedConstruction(data->name, data->fields);
            }
        }
        while (peek() == "(" || peek() == "." || peek() == "[")
        {
            if (peek() == "(")
            {
                left += "(" + arguments() + ")";
            }
            else if (take("["))
            {
                const std::string index = expr();
                need("]");
                left = "(" + left + ")[" + index + "]";
            }
            else
            {
                need(".");
                const std::string member = name();
                if (member == "rs_len" && peek() == "(")
                {
                    need("(");
                    need(")");
                    left = "nr_len(" + left + ")";
                }
                else
                {
                    left += "." + member;
                }
            }
        }
        while (prec(peek()) >= min)
        {
            const std::string op = peek();
            ++p;
            const std::string right = expr(prec(op) + 1, allowConstruction);
            left = "(" + left + " " + op + " " + right + ")";
        }
        return left;
    }

    std::string match(bool value, bool returnTail = false)
    {
        const std::string subject = expr(1, false);
        if (returnTail)
        {
            value = finalMatch();
        }
        need("{");
        const std::string id = std::to_string(serial++);
        const std::string variable = "nr_match_" + id;
        const std::string payload = "nr_payload_" + id;
        std::string result = returnTail && value ? "return " : "";
        result += value ? "([&]() {\n" : "{\n";
        result += "auto&& " + variable + " = " + subject + ";\n";
        bool first = true;
        bool wildcard = false;
        while (!take("}"))
        {
            if (wildcard)
            {
                fail("wildcard match arm must be last");
            }
            std::string bindings;
            if (take("_"))
            {
                wildcard = true;
                result += first ? "{\n" : "else {\n";
            }
            else
            {
                const std::string enumName = take("Self") ? implType : name();
                const DataType* data = findType(enumName);
                if (data == nullptr || !data->isEnum)
                {
                    fail("match currently requires Enum::Variant patterns or _");
                }
                need("::");
                const Variant& variant = findVariant(*data, name());
                const std::string caseType = enumName + "::nr_case_" + variant.name;
                result += first ? "if (" : "else if (";
                result += "const " + caseType + "* " + payload + " = std::get_if<" + caseType +
                          ">(&" + variable + ".nr_value)) {\n";
                if (variant.kind == VariantKind::Tuple)
                {
                    need("(");
                    size_t index = 0;
                    while (!take(")"))
                    {
                        if (index == variant.fields.size())
                        {
                            fail("too many tuple pattern bindings");
                        }
                        take("mut");
                        if (!take("_"))
                        {
                            const std::string binding = name();
                            bindings += "auto " + binding + " = " + payload + "->" +
                                        variant.fields[index].name + ";\n";
                        }
                        ++index;
                        if (!take(","))
                        {
                            need(")");
                            break;
                        }
                    }
                    if (index != variant.fields.size())
                    {
                        fail("tuple pattern needs one binding per field");
                    }
                }
                else if (variant.kind == VariantKind::Named)
                {
                    need("{");
                    std::vector<std::string> seen;
                    bool rest = false;
                    while (!take("}"))
                    {
                        if (take(".."))
                        {
                            rest = true;
                            take(",");
                            need("}");
                            break;
                        }
                        const std::string field = name();
                        bool found = false;
                        for (const Field& declared : variant.fields)
                        {
                            found = found || declared.name == field;
                        }
                        for (const std::string& previous : seen)
                        {
                            if (previous == field)
                            {
                                fail("repeated pattern field");
                            }
                        }
                        if (!found)
                        {
                            fail("unknown pattern field");
                        }
                        seen.push_back(field);
                        std::string binding = field;
                        if (take(":"))
                        {
                            if (take("_"))
                            {
                                binding.clear();
                            }
                            else
                            {
                                take("mut");
                                binding = name();
                            }
                        }
                        if (!binding.empty())
                        {
                            bindings += "auto " + binding + " = " + payload + "->" + field + ";\n";
                        }
                        if (!take(","))
                        {
                            need("}");
                            break;
                        }
                    }
                    if (!rest && seen.size() != variant.fields.size())
                    {
                        fail("named pattern needs every field or ..");
                    }
                }
            }
            need("=>");
            result += bindings;
            const bool armBlock = peek() == "{";
            if (value)
            {
                ++valueDepth;
            }
            if (armBlock)
            {
                result += block(value, value);
            }
            else
            {
                result += (value ? "return " : "") + expr() + ";\n";
            }
            if (value)
            {
                --valueDepth;
            }
            result += "}\n";
            first = false;
            if (!take(",") && !armBlock && peek() != "}")
            {
                fail("expected ',' after match arm");
            }
        }
        if (first)
        {
            fail("empty matches are not supported yet");
        }
        if (!wildcard)
        {
            result += "else { throw std::runtime_error(\"non-exhaustive match\"); }\n";
        }
        return result + (value ? "}())" : "}\n") + (returnTail && value ? ";\n" : "");
    }
    std::string print()
    {
        need("println");
        need("!");
        need("(");
        if (take(")"))
        {
            need(";");
            return "std::cout << '\\n';\n";
        }
        if (peek()[0] != '"')
        {
            fail("println! needs a literal format string");
        }
        const std::string format = peek();
        ++p;
        std::vector<std::string> args;
        while (take(",") && peek() != ")")
        {
            args.push_back(expr());
        }
        need(")");
        need(";");
        std::string result = "{\n";
        for (size_t i = 0; i < args.size(); ++i)
        {
            result += "auto nr_arg_" + std::to_string(i) + " = " + args[i] + ";\n";
        }
        result += "std::cout";
        std::string literal;
        size_t arg = 0;
        for (size_t i = 1; i + 1 < format.size(); ++i)
        {
            char c = format[i];
            if ((c == '{' || c == '}') && i + 2 < format.size() && format[i + 1] == c)
            {
                literal += c;
                ++i;
            }
            else if (c == '{' && i + 2 < format.size() && format[i + 1] == '}')
            {
                if (arg == args.size())
                {
                    fail("not enough println! arguments");
                }
                result += " << \"" + literal + "\" << nr_arg_" + std::to_string(arg++);
                literal.clear();
                ++i;
            }
            else if (c == '{' || c == '}')
            {
                fail("only {} placeholders and escaped braces supported");
            }
            else
            {
                literal += c;
            }
        }
        if (arg != args.size())
        {
            fail("too many println! arguments");
        }
        return result + " << \"" + literal + "\" << '\\n';\n}\n";
    }
    std::string conditional(bool value = false)
    {
        const std::string cond = expr(1, false);
        std::string result = "if (" + cond + ") " + block(value, value);
        if (take("else"))
        {
            result += "else " + (take("if") ? conditional(value) : block(value, value));
        }
        else if (value)
        {
            fail("a value-producing if requires else");
        }
        return result;
    }

    bool finalConditional() const
    {
        size_t index = p;
        do
        {
            size_t parentheses = 0;
            while (ts[index].text != "<eof>")
            {
                if (ts[index].text == "(")
                {
                    ++parentheses;
                }
                else if (ts[index].text == ")" && parentheses != 0)
                {
                    --parentheses;
                }
                else if (ts[index].text == "{" && parentheses == 0)
                {
                    break;
                }
                ++index;
            }
            if (ts[index].text == "<eof>")
            {
                return false;
            }
            size_t depth = 1;
            ++index;
            while (depth != 0 && ts[index].text != "<eof>")
            {
                if (ts[index].text == "{")
                {
                    ++depth;
                }
                else if (ts[index].text == "}")
                {
                    --depth;
                }
                ++index;
            }
            if (ts[index].text != "else")
            {
                return depth == 0 && ts[index].text == "}";
            }
            ++index;
        } while (ts[index].text != "<eof>");
        return false;
    }
    bool finalMatch() const
    {
        size_t index = p;
        size_t parentheses = 0;
        while (ts[index].text != "<eof>")
        {
            if (ts[index].text == "(")
            {
                ++parentheses;
            }
            else if (ts[index].text == ")" && parentheses != 0)
            {
                --parentheses;
            }
            else if (ts[index].text == "{" && parentheses == 0)
            {
                size_t depth = 1;
                ++index;
                while (depth != 0 && ts[index].text != "<eof>")
                {
                    if (ts[index].text == "{")
                    {
                        ++depth;
                    }
                    else if (ts[index].text == "}")
                    {
                        --depth;
                    }
                    ++index;
                }
                return depth == 0 && ts[index].text == "}";
            }
            ++index;
        }
        return false;
    }

    std::string block(bool tail = false, bool expressionBlock = false)
    {
        need("{");
        std::string out = "{\n";
        while (!take("}"))
        {
            if (peek() == "<eof>")
            {
                fail("unterminated block");
            }
            if (take("let"))
            {
                take("mut");
                const std::string n = name();
                const std::string t = take(":") ? type() : "auto";
                need("=");
                std::string value = expr();
                need(";");
                out += t + " " + n + " = " + value + ";\n";
            }
            else if (peek() == "println")
            {
                out += print();
            }
            else if (take("return"))
            {
                if (valueDepth != 0)
                {
                    fail("explicit return inside a match value is not supported yet");
                }
                out += "return";
                if (peek() != ";")
                {
                    out += " " + expr();
                }
                need(";");
                out += ";\n";
            }
            else if (take("if"))
            {
                out += conditional(tail && (returnType != "void" || expressionBlock) &&
                                   finalConditional());
            }
            else if (take("match"))
            {
                out += match(false, tail && (returnType != "void" || expressionBlock));
                take(";");
            }
            else if (take("while"))
            {
                const std::string cond = expr(1, false);
                continueActions.push_back("");
                out += "while (" + cond + ") " + block();
                continueActions.pop_back();
            }
            else if (take("loop"))
            {
                continueActions.push_back("");
                out += "while (true) " + block();
                continueActions.pop_back();
            }
            else if (take("continue"))
            {
                if (valueDepth != 0)
                {
                    fail("continue inside a value expression is not supported yet");
                }
                need(";");
                // Inclusive ranges increment in their epilogue; jumping past it would repeat
                // forever.
                if (!continueActions.empty())
                {
                    out += continueActions.back();
                }
                out += "continue;\n";
            }
            else if (take("for"))
            {
                const std::string n = name();
                need("in");
                const std::string start = expr();
                bool inclusive = take("..=");
                if (!inclusive)
                {
                    need("..");
                }
                const std::string end = expr();
                const std::string id = std::to_string(serial++);
                out += "{\nauto nr_start_" + id + " = " + start + ";\nauto nr_end_" + id + " = " +
                       end + ";\n";
                out += "for (auto " + n + " = nr_start_" + id + "; " + n +
                       (inclusive ? " <= " : " < ") + "nr_end_" + id + "; " +
                       (inclusive ? "" : "++" + n) + ") {\n";
                const std::string increment =
                    inclusive ? "if (" + n + " == nr_end_" + id + ") break;\n++" + n + ";\n" : "";
                continueActions.push_back(increment);
                out += block();
                continueActions.pop_back();
                // Avoid overflowing the last value of an inclusive range.
                if (inclusive)
                {
                    out += increment;
                }
                out += "}\n}\n";
            }
            else if (take("break"))
            {
                if (valueDepth != 0)
                {
                    fail("break inside a match value is not supported yet");
                }
                need(";");
                out += "break;\n";
            }
            else
            {
                std::string value = expr();
                if (peek() == "=" || peek() == "+=" || peek() == "-=" || peek() == "*=" ||
                    peek() == "/=")
                {
                    const std::string op = peek();
                    ++p;
                    value += " " + op + " " + expr();
                }
                if (take(";"))
                {
                    out += value + ";\n";
                }
                else if (tail && peek() == "}" && (returnType != "void" || expressionBlock))
                {
                    out += "return " + value + ";\n";
                }
                else
                {
                    fail("expected ';' (block expressions unsupported)");
                }
            }
        }
        return out + "}\n";
    }

    std::string signature(const Function& function, bool declaration) const
    {
        std::string result;
        if (declaration && !function.owner.empty() && !function.receiver)
        {
            result = "static ";
        }
        result += function.result + " ";
        if (!declaration && !function.owner.empty())
        {
            result += function.owner + "::";
        }
        result += function.name + "(" + function.parameters + ")";
        if (function.constReceiver)
        {
            result += " const";
        }
        return result;
    }

    static std::string emitFields(const std::vector<Field>& fields)
    {
        std::string result;
        for (const Field& field : fields)
        {
            result += field.type + " " + field.name + ";\n";
        }
        return result;
    }

    std::string emitType(const DataType& data) const
    {
        std::string result = "struct " + data.name + " {\n";
        if (!data.isEnum)
        {
            result += emitFields(data.fields);
        }
        else
        {
            for (const Variant& variant : data.variants)
            {
                result +=
                    "struct nr_case_" + variant.name + " {\n" + emitFields(variant.fields) + "};\n";
            }
            result += "std::variant<";
            for (size_t i = 0; i < data.variants.size(); ++i)
            {
                if (i != 0)
                {
                    result += ", ";
                }
                result += "nr_case_" + data.variants[i].name;
            }
            result += "> nr_value;\n";
        }
        for (const Function& function : functions)
        {
            if (function.owner == data.name)
            {
                result += signature(function, true) + ";\n";
            }
        }
        return result + "};\n\n";
    }

    std::string emitTypes() const
    {
        std::string result;
        for (const DataType& data : dataTypes)
        {
            result += "struct " + data.name + ";\n";
        }
        // By-value fields need their dependencies defined first; references need only a forward
        // declaration.
        std::vector<bool> emitted(dataTypes.size(), false);
        size_t count = 0;
        while (count < dataTypes.size())
        {
            bool progress = false;
            for (size_t i = 0; i < dataTypes.size(); ++i)
            {
                if (emitted[i])
                {
                    continue;
                }
                const DataType& data = dataTypes[i];
                std::vector<Field> dependencies = data.fields;
                for (const Variant& variant : data.variants)
                {
                    dependencies.insert(dependencies.end(), variant.fields.begin(),
                                        variant.fields.end());
                }
                bool ready = true;
                for (const Field& field : dependencies)
                {
                    for (size_t j = 0; j < dataTypes.size(); ++j)
                    {
                        if (field.type == dataTypes[j].name && !emitted[j])
                        {
                            ready = false;
                        }
                    }
                }
                if (ready)
                {
                    result += emitType(data);
                    emitted[i] = true;
                    ++count;
                    progress = true;
                }
            }
            if (!progress)
            {
                fail("recursive by-value types cannot be emitted");
            }
        }
        return result;
    }

  public:
    explicit Parser(const std::string& s) : ts(lex(s))
    {
    }
    std::string run()
    {
        while (peek() != "<eof>")
        {
            take("pub");
            if (take("struct"))
            {
                dataDeclaration(false);
            }
            else if (take("enum"))
            {
                dataDeclaration(true);
            }
            else if (take("impl"))
            {
                const std::string owner = name();
                need("{");
                while (!take("}"))
                {
                    take("pub");
                    need("fn");
                    functionDeclaration(owner);
                }
            }
            else
            {
                need("fn");
                functionDeclaration();
            }
        }
        std::string declarations = emitTypes();
        std::string bodies;
        bool main = false;
        for (const Function& function : functions)
        {
            p = function.body;
            returnType = function.result;
            implType = function.owner;
            hasReceiver = function.receiver;
            if (!function.owner.empty() && findType(function.owner) == nullptr)
            {
                fail("impl target must be a declared struct or enum");
            }
            if (function.owner.empty())
            {
                declarations += signature(function, true) + ";\n";
                if (function.name == "rs_main")
                {
                    if (function.result != "void" || !function.parameters.empty())
                    {
                        fail("main must have no parameters or return value");
                    }
                    main = true;
                }
            }
            bodies += signature(function, false) + " " + block(true) + "\n";
        }
        return "// Generated by no-rust. Zero-cost assumptions.\n#include <cstdint>\n#include "
               "<cstddef>\n#include <iostream>\n#include <string>\n#include <variant>\n"
               "#include <utility>\n#include <stdexcept>\n#include <array>\n\n"
               "template<std::size_t N, typename T>\n"
               "std::array<T, N> nr_repeat(T value) {\n"
               "std::array<T, N> result{};\nresult.fill(value);\nreturn result;\n}\n\n"
               "template<typename T>\ndecltype(auto) nr_len(T&& value) {\n"
               "if constexpr (requires { value.rs_len(); }) { return value.rs_len(); }\n"
               "else { return value.size(); }\n}\n\n" +
               declarations + "\n" + bodies +
               (main ? "int main() {\nstd::cout << std::boolalpha;\nrs_main();\n}\n" : "");
    }
};
} // namespace
int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--help")
    {
        std::cout << "no-rust <input.rs> [-o output.cpp]\nZero-cost assumptions.\n";
        return 0;
    }
    if (argc != 2 && !(argc == 4 && std::string(argv[2]) == "-o"))
    {
        std::cerr << "usage: no-rust <input.rs> [-o output.cpp]\n";
        return 1;
    }
    try
    {
        std::ifstream input(argv[1], std::ios::binary);
        if (!input)
        {
            throw std::runtime_error("cannot open input");
        }
        std::ostringstream s;
        s << input.rdbuf();
        const std::string output = Parser(s.str()).run();
        if (argc == 4)
        {
            std::ofstream file(argv[3], std::ios::binary);
            if (!file || !(file << output))
            {
                throw std::runtime_error("cannot write output");
            }
        }
        else
        {
            std::cout << output;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << argv[1] << ":" << e.what() << '\n';
        return 1;
    }
}
