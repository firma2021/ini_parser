#pragma once

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <spanstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

using namespace std;

struct IniParseError : runtime_error
{
    static string format_error_info(string_view err, size_t line_no)
    {
        try
        {
            ostringstream oss;
            oss.exceptions(ios::failbit | ios::badbit);
            oss << err << " at line " << line_no;
            return std::move(oss).str();
        }
        catch (...)
        {
            return {};
        }
    }

    IniParseError(string_view err, size_t line_no) : runtime_error {format_error_info(err, line_no)}
    {}
};

class IniResult
{
    friend class IniParser;

    string raw_content;
    map<string_view, map<string_view, string_view>> parsed_data;

    auto insert(string_view section_name) { return parsed_data.try_emplace(section_name).first; }

    void insert(auto section_iter, string_view key, string_view value)
    {
        section_iter->second.insert_or_assign(key, value);
    }

    template <typename T>
    static optional<T> string_to(string_view str)
    {
        if constexpr (is_same_v<T, string>) { return string {str}; }
        else if constexpr (is_same_v<T, bool>)
        {
            string lower_str;
            lower_str.reserve(size(str));
            auto op = [](unsigned char c) { return static_cast<unsigned char>(tolower(c)); };
            ranges::transform(str, back_inserter(lower_str), op);

            static const unordered_set true_strings {"true"s, "t"s, "yes"s, "y"s};
            static const unordered_set false_strings {"false"s, "f"s, "no"s, "n"s};

            if (true_strings.contains(lower_str)) { return true; }
            if (false_strings.contains(lower_str)) { return false; }

            return nullopt;
        }
        else if constexpr (is_arithmetic_v<T>)
        {
            T number {};
            auto [ptr, ec] {from_chars(data(str), data(str) + size(str), number)};

            if (ec != errc {} || ptr != data(str) + size(str)) { return nullopt; }

            return number;
        }
        else
        {
            ispanstream iss {str};
            T value;
            if (iss >> value && iss.eof()) { return move(value); }

            return nullopt;
        }
    }

public:
    explicit IniResult(string raw_content_) : raw_content {std::move(raw_content_)} {}

    string_view view() { return raw_content; }

    [[nodiscard]]
    bool contains(string_view section_name) const
    {
        return parsed_data.contains(section_name);
    }

    [[nodiscard]]
    bool contains(string_view section_name, string_view key) const
    {
        auto iter {parsed_data.find(section_name)};
        return iter != cend(parsed_data) && iter->second.contains(key);
    }

    [[nodiscard]]
    optional<string_view> get(string_view section_name, string_view key) const
    {
        if (auto iter {parsed_data.find(section_name)}; iter != end(parsed_data))
        {
            const auto& section {iter->second};
            if (auto kv_iter {section.find(key)}; kv_iter != end(section))
            {
                return kv_iter->second;
            }
        }
        return nullopt;
    }

    template <typename T>
    [[nodiscard]]
    optional<T> get(string_view section_name, string_view key) const
    {
        if (auto value {get(section_name, key)}; value.has_value()) { return string_to<T>(*value); }
        return nullopt;
    }
};

class IniParser
{
    enum struct Token : char
    {
        end,
        opened_square_brace,
        closed_square_brace,
        equal,
        text
    };

    class TokenScanner
    {
        string_view source;

        string_view::iterator cur;

        size_t line_no {1};

        string_view text;

        void skip_bom()
        {
            const array<unsigned char, 3> boms {0xef, 0xbb, 0xbf};
            if (ranges::equal(boms, source)) { cur += size(boms); }
        }

        void skip_comment()
        {
            ++cur;
            while (cur != end(source))
            {
                switch (*cur)
                {
                case '\n':
                    ++cur;
                    ++line_no;
                    return;
                default:
                    ++cur;
                    continue;
                }
            }
        }

        // parameter 'end_char' with unknown value cannot be used in a constant expression
        template <char end_char>
        Token scan_quoted_string()
        {
            const auto* str_begin = ++cur;

            while (cur != end(source))
            {
                switch (*cur)
                {
                case '\n':
                    throw IniParseError {"Unclosed string", line_no};
                case end_char:
                    {
                        const auto* str_end = cur;
                        ++cur;
                        text = {str_begin, str_end};
                        return Token::text;
                    }
                default:
                    ++cur;
                    continue;
                }
            }

            throw IniParseError {"Unclosed string", line_no};
        }

        Token scan_unquoted_word()
        {
            const auto* str_begin = cur;

            while (cur != end(source))
            {
                switch (*cur)
                {
                case ' ':
                case '\t':
                case '\r':
                case '\n':
                case '=':
                case '"':
                case '\'':
                case '#':
                case ';':
                    text = {str_begin, cur};
                    return Token::text;
                default:
                    ++cur;
                    continue;
                }
            }

            throw IniParseError {"Unclosed string", line_no};
        }

    public:
        explicit TokenScanner(string_view source_) : source {source_}, cur {begin(source)}
        {
            skip_bom();
        }

        Token next()
        {
            while (cur != end(source))
            {
                switch (*cur)
                {
                case ' ':
                case '\t':
                case '\r':
                    ++cur;
                    continue;
                case '\n':
                    ++line_no;
                    ++cur;
                    continue;
                case '#':
                case ';':
                    skip_comment();
                    continue;
                case '[':
                    ++cur;

                    return Token::opened_square_brace;
                case ']':
                    ++cur;
                    return Token::closed_square_brace;
                case '=':
                    ++cur;
                    return Token::equal;
                case '"':
                    return scan_quoted_string<'"'>();
                case '\'':
                    return scan_quoted_string<'\''>();
                default:
                    return scan_unquoted_word();
                }
            }

            return Token::end;
        }

        [[nodiscard]]
        string_view text_view() const
        {
            return text;
        }

        [[nodiscard]]
        size_t line_index() const
        {
            return line_no;
        }
    };

    static string read_istream(istream& in)
    {
        auto istream_begin_pos = in.tellg();
        if (istream_begin_pos == -1) { return (ostringstream {} << in.rdbuf()).str(); }

        in.seekg(0, ios::end);
        auto istream_size = in.tellg() - istream_begin_pos;
        in.seekg(istream_begin_pos);

        string source;
        source.resize_and_overwrite(static_cast<size_t>(istream_size),
                                    [&in](auto* p, auto n)
                                    { return in.read(p, static_cast<streamsize>(n)).gcount(); });
        return source;
    }

    static string read_file(const filesystem::path& filename)
    {
        ifstream fin {filename};
        fin.exceptions(ios::failbit | ios::badbit);
        return read_istream(fin);
    }

    static auto parse_section_name(TokenScanner& scanner, IniResult& ini_result)
    {
        auto line_index = scanner.line_index();
        if (scanner.next() != Token::text)
        {
            throw IniParseError {"invalid section name", line_index};
        }
        if (scanner.next() != Token::closed_square_brace)
        {
            throw IniParseError {"invalid section name: missing close square brace", line_index};
        }

        return ini_result.insert(scanner.text_view());
    }

    static void parse_property(TokenScanner& scanner, IniResult& ini_result, auto section_iter)
    {
        string_view key = scanner.text_view();

        if (scanner.next() != Token::equal)
        {
            throw IniParseError {"expected equal after parameter name", scanner.line_index()};
        }
        if (scanner.next() != Token::text)
        {
            throw IniParseError {"expected parameter value", scanner.line_index()};
        }

        string_view value = scanner.text_view();

        ini_result.insert(section_iter, key, value);
    }

public:
    IniParser() = delete;

    static IniResult parse_str(string source)
    {
        IniResult result {std::move(source)};

        TokenScanner scanner {result.view()};

        auto section_iter = end(result.parsed_data);

        Token token {};
        while (token = scanner.next(), token != Token::end)
        {
            switch (token)
            {
            case Token::opened_square_brace:
                section_iter = parse_section_name(scanner, result);
                break;
            case Token::text:
                parse_property(scanner, result, section_iter);
                break;
            default:
                throw IniParseError {"expected section or parameter", scanner.line_index()};
            }
        };

        return result;
    }

    static IniResult parse(istream& in) { return parse_str(read_istream(in)); }

    static IniResult parse(const filesystem::path& filename)
    {
        return parse_str(read_file(filename));
    }
};
