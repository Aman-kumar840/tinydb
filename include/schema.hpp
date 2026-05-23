#pragma once
#include <string>
#include <vector>
#include <cstdint>
using namespace std;
enum class ColType : uint8_t
{
    INT,
    UINT,
    VARCHAR
};

struct Column
{
    string name;
    ColType type;
    uint16_t size; // Only used if VARCHAR; 4 for others
};

struct TableSchema
{
    string table_name;
    vector<Column> columns;
    uint16_t row_size = 0; // Total bytes per row
};