# 现代 C++ INI 解析器

一个纯头文件的，简单高效的INI文件解析器，支持读取节和键值对，并且能够将值转换为多种类型。



- 采用现代 C++20 标准实现。无依赖库。
- 高效的单遍扫描词法分析。
- 使用字符串视图引用原始内容，只动态分配一次内存。



## 支持的INI格式

```ini
; 使用分号作为注释
# 或者使用井号作为注释

[Section1]
string_key=Hello_World
quoted="Hello ; World"
single_quoted='Hello # World'

[Section2]
int_key = 42
bool_key= true
float_key=3.14
```

- 支持UTF-8 BOM。键只支持ASCII字符，值可以是任何UTF-8字符。
- 支持重复的节和重复的键，后面的值会覆盖前面的值。

## 使用方法

从文件解析：
```c++
IniResult result = IniParser::parse("path/to/config.ini");
```

从输入流解析：
```c++
std::ifstream fin("path/to/config.ini");
IniResult result = IniParser::parse(fin);
```

从字符串解析：
```c++
std::string ini_content = "[section]\nkey=value";
IniResult result = IniParser::parse_str(ini_content);
```



如果发生任何解析错误（错误的ini语法），将抛出 IniParseError 异常。

```cpp
try
{
    IniResult result = IniParser::parse("path/to/config.ini");
} catch (const IniParseError& e)
{
    std::cerr << "解析错误: " << e.what() << std::endl;
}
```



成功解析INI文件后返回IniResult对象，可以使用它来访问解析后的数据。



检查节是否存在：

```cpp
if (result.contains("section_name"))
{
    // 节存在
}
```



检查某个节内的键是否存在：

```cpp
if (result.contains("section_name", "key_name"))
{
    // 键在指定的节中存在
}
```



获取与节中的键相关联的值：

`get()`方法返回`string_view`类型的值：

```c++
auto value = result.get("section_name", "key_name");
if (value)
{
    std::cout << "值: " << *value << std::endl;
}
```

`get<T>()`方法返回`optional<T>`类型的值：

```cpp
auto intValue = result.get<int>("section_name", "integer_key");
if (intValue)
{
    std::cout << "整数值: " << *intValue << std::endl;
}

auto boolValue = result.get<bool>("section_name", "boolean_key");
if (boolValue)
{
    std::cout << "布尔值: " << (*boolValue ? "true" : "false") << std::endl;
}
```
