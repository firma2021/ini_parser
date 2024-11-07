#pragma once

#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
using namespace std;

class IniGenerator
{
private:
    map<string, pair<string, map<string, pair<string, string>>>> ini_data;
    pair<string, map<string, pair<string, string>>>* current_section {nullptr};

    [[nodiscard]]
    bool no_current_section() const
    {
        return current_section == nullptr;
    }

public:
    IniGenerator& set(string section_name, string comment)
    {
        current_section = &(ini_data[std::move(section_name)]);
        current_section->first = std::move(comment);

        return *this;
    }

    IniGenerator& set(string key, string value, string comment)
    {
        if (no_current_section()) { throw invalid_argument {"No current section set"}; }
        current_section->second[std::move(key)] = {std::move(value), std::move(comment)};

        return *this;
    }

    IniGenerator& set(string section_name, string key, string value, string field_comment)
    {
        set(std::move(section_name), "");
        set(std::move(key), std::move(value), std::move(field_comment));
        return *this;
    }

    void generate(ostream& os) const
    {
        for (const auto& [section_name, section_data] : ini_data)
        {
            const auto& [section_comment, section_field] = section_data;
            os << '#' << ' ' << section_comment << '\n';
            os << '[' << section_name << ']' << '\n';

            for (const auto& [k, field_data] : section_field)
            {
                const auto& [v, field_comment] = field_data;
                os << k << " = " << v << ' ' << '#' << ' ' << field_comment << '\n';
            }

            os.put('\n');
        }
    }

    ofstream generate(const filesystem::path& file_name) const
    {
        ofstream fout {file_name};
        if (fout.fail()) { throw runtime_error {"Failed to open file for writing"}; }

        generate(fout);

        fout.flush();
        return fout;
    }
};
