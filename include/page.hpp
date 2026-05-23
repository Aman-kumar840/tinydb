#pragma once
#include "row.hpp"
#include <array>
#include <cstdint>
using namespace std;
inline constexpr size_t PAGE_SIZE = 4096;
inline constexpr size_t HEADER_BYTES = sizeof(uint16_t) * 2;
inline constexpr size_t ROWS_PER_PAGE = (PAGE_SIZE - HEADER_BYTES) / sizeof(Row);
inline constexpr uint16_t FREE_NONE = 0xFFFF;

struct PageHeader
{
    uint16_t row_count = 0;
    uint16_t free_head = FREE_NONE;
};

struct Page
{
    PageHeader header;
    array<Row, ROWS_PER_PAGE> rows{};
};