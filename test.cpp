#include "ini_parser.hpp"
#include <cassert>
#include <iostream>
#include <spanstream>
#include <sstream>

void test_ini_parser()
{
    // 测试基本解析功能
    {
        std::string ini_content = R"(
[section1]
key1 = value1
key2 = 123

[section2]
key3 = true
key4 = 3.14
)";
        IniResult result = IniParser::parse_str(std::move(ini_content));

        assert(result.contains("section1"));
        assert(result.contains("section2"));
        assert(result.contains("section1", "key1"));
        assert(result.contains("section1", "key2"));
        assert(result.contains("section2", "key3"));
        assert(result.contains("section2", "key4"));

        assert(result.get("section1", "key1") == "value1");
        assert(result.get<int>("section1", "key2") == 123);
        assert(result.get<bool>("section2", "key3") == true);
        assert(result.get<double>("section2", "key4") == 3.14);
    }

    // 测试注释和空行
    {
        std::string ini_content = R"(
    ; This is a comment
    # This is also a comment

    [section]
    key = value ; inline comment
    )";
        std::istringstream iss(ini_content);
        IniResult result = IniParser::parse(iss);

        assert(result.contains("section"));
        assert(result.get("section", "key") == "value");
    }

    // 测试引号字符串
    {
        std::string ini_content = R"(
    [section]
    key1 = "value with spaces"
    key2 = 'another value'
    key3 = 'another ; # value'
    )";
        std::ispanstream iss(ini_content);
        IniResult result = IniParser::parse(iss);

        assert(result.get("section", "key1") == "value with spaces");
        assert(result.get("section", "key2") == "another value");
        assert(result.get("section", "key3") == "another ; # value");
    }

    // 测试错误处理
    {
        std::string invalid_ini = R"(
    [unclosed_section
    key = value
    )";
        std::istringstream iss(invalid_ini);
        try
        {
            IniParser::parse(iss);
            assert(false && "Expected an exception");
        }
        catch (const std::exception& e)
        {
            std::cout << "Caught expected exception: " << e.what() << '\n';
        }
    }

    // 测试不存在的键和节
    {
        std::string ini_content = "[section]\nkey = value\n";
        std::istringstream iss(ini_content);
        IniResult result = IniParser::parse(iss);

        assert(!result.contains("non_existent_section"));
        assert(!result.contains("section", "non_existent_key"));
        assert(!result.get("section", "non_existent_key").has_value());
    }

    // 测试类型转换
    {
        std::string ini_content = R"(
    [section]
    int = 42
    float = 3.14
    bool_true = true
    bool_false = false
    string = hello
    )";
        std::istringstream iss(ini_content);
        IniResult result = IniParser::parse(iss);

        assert(result.get<int>("section", "int") == 42);
        assert(result.get<float>("section", "float") == 3.14F);
        assert(result.get<bool>("section", "bool_true") == true);
        assert(result.get<bool>("section", "bool_false") == false);
        assert(result.get<std::string>("section", "string") == "hello");

        // 测试无效转换
        assert(!result.get<int>("section", "string").has_value());
        assert(!result.get<bool>("section", "int").has_value());
    }

    std::cout << "All tests passed successfully!" << '\n';
}

int main()
{
    test_ini_parser();
}
