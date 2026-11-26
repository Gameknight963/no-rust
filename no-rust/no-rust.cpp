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
            for (const char* op :
                 {"..=", "->", "==", "!=", "<=", ">=", "&&", "||", "+=", "-=", "*=", "/=", ".."})
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
                if (std::string("(){};,:+-*/%!=<>").find(s[i]) == std::string::npos)
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
    std::vector<Token> ts;
    size_t p = 0, serial = 0;
    std::string returnType;
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
        fail("unsupported type");
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
    std::string expr(int min = 1)
    {
        std::string left;
        if (peek() == "!" || peek() == "-")
        {
            const std::string op = peek();
            ++p;
            left = "(" + op + expr(7) + ")";
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
            left = name();
            if (take("("))
            {
                left += "(";
                if (!take(")"))
                {
                    bool first = true;
                    do
                    {
                        if (!first)
                        {
                            left += ", ";
                        }
                        first = false;
                        left += expr();
                    } while (take(",") && peek() != ")");
                    need(")");
                }
                left += ")";
            }
        }
        while (prec(peek()) >= min)
        {
            const std::string op = peek();
            ++p;
            const std::string right = expr(prec(op) + 1);
            left = "(" + left + " " + op + " " + right + ")";
        }
        return left;
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
    std::string conditional()
    {
        const std::string cond = expr();
        std::string result = "if (" + cond + ") " + block();
        if (take("else"))
        {
            result += "else " + (take("if") ? conditional() : block());
        }
        return result;
    }
    std::string block(bool tail = false)
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
                out += conditional();
            }
            else if (take("while"))
            {
                const std::string cond = expr();
                out += "while (" + cond + ") " + block();
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
                out += block();
                // Avoid overflowing the last value of an inclusive range.
                if (inclusive)
                {
                    out += "if (" + n + " == nr_end_" + id + ") break;\n++" + n + ";\n";
                }
                out += "}\n}\n";
            }
            else if (take("break"))
            {
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
                else if (tail && peek() == "}" && returnType != "void")
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

  public:
    explicit Parser(const std::string& s) : ts(lex(s))
    {
    }
    std::string run()
    {
        std::string declarations, bodies;
        bool main = false;
        while (peek() != "<eof>")
        {
            take("pub");
            need("fn");
            const std::string n = name();
            need("(");
            std::string params;
            if (!take(")"))
            {
                do
                {
                    take("mut");
                    const std::string param = name();
                    need(":");
                    const std::string t = type();
                    if (!params.empty())
                    {
                        params += ", ";
                    }
                    params += t + " " + param;
                } while (take(",") && peek() != ")");
                need(")");
            }
            returnType = take("->") ? type() : "void";
            if (n == "rs_main")
            {
                if (returnType != "void" || !params.empty())
                {
                    fail("main must have no parameters or return value");
                }
                main = true;
            }
            const std::string signature = returnType + " " + n + "(" + params + ")";
            declarations += signature + ";\n";
            bodies += signature + " " + block(true) + "\n";
        }
        return "// Generated by no-rust. Zero-cost assumptions.\n#include <cstdint>\n#include "
               "<cstddef>\n#include <iostream>\n#include <string>\n\n" +
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
