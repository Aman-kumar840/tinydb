#include "../include/schema.hpp"
#include <regex>
#include <fstream>
#include <sstream>
#include <iostream>
using namespace std;
bool create_table_from_command(const string &input)
{
    regex table_re(R"(create\s+table\s+(\w+)\s*\((.+)\))", regex::icase);
    smatch match;

    if (!regex_search(input, match, table_re))
    {
        cerr << "Error: Invalid CREATE TABLE syntax.\n";
        return false;
    }

    string table_name = match[1];
    string columns_str = match[2];

    regex col_re(R"(\s*(\w+)\s+(uint|int|varchar(?:\((\d+)\))?)\s*,?)", regex::icase);
    auto cols_begin = sregex_iterator(columns_str.begin(), columns_str.end(), col_re);
    auto cols_end = sregex_iterator();

    TableSchema schema;
    schema.table_name = table_name;

    for (auto it = cols_begin; it != cols_end; ++it)
    {
        Column col;
        col.name = (*it)[1];

        string type_str = (*it)[2];
        if (type_str.find("varchar") == 0)
        {
            col.type = ColType::VARCHAR;
            col.size = (*it)[3].matched ? stoi((*it)[3]) : 64; // default size
            if (col.size <= 0 || col.size > 255)
            {
                cerr << "Error: Invalid varchar size.\n";
                return false;
            }
        }
        else if (type_str == "uint")
        {
            col.type = ColType::UINT;
            col.size = 4;
        }
        else if (type_str == "int")
        {
            col.type = ColType::INT;
            col.size = 4;
        }
        else
        {
            cerr << "Error: Unknown column type.\n";
            return false;
        }

        schema.columns.push_back(col);
        schema.row_size += col.size;
    }

    // Write schema to file
    string schema_path = table_name + ".schema";
    ofstream out(schema_path, ios::binary);
    uint16_t ncols = schema.columns.size();
    out.write(reinterpret_cast<const char *>(&ncols), sizeof(ncols));
    for (const auto &col : schema.columns)
    {
        uint16_t len = col.name.size();
        out.write(reinterpret_cast<const char *>(&len), sizeof(len));
        out.write(col.name.data(), len);
        out.write(reinterpret_cast<const char *>(&col.type), sizeof(col.type));
        out.write(reinterpret_cast<const char *>(&col.size), sizeof(col.size));
    }

    return true;
}