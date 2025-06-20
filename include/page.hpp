#pragma once
#include "row.hpp"
#include <array>
#include <cstdint>

/* ──────────  Constants  ────────── */
inline constexpr std::size_t PAGE_SIZE = 4096;                    // 4 KB page
inline constexpr std::size_t HEADER_BYTES = sizeof(uint16_t) * 2; // row_count + free_head
inline constexpr std::size_t ROWS_PER_PAGE = (PAGE_SIZE - HEADER_BYTES) / sizeof(Row);
inline constexpr uint16_t FREE_NONE = 0xFFFF;

/* ──────────  On‑disk page header  ────────── */
struct PageHeader
{
    uint16_t row_count = 0;         // how many rows used in this page
    uint16_t free_head = FREE_NONE; // (not used yet, for future free‑list)
};

/* ──────────  In‑memory helper  ────────── */
struct Page
{
    PageHeader header;
    std::array<Row, ROWS_PER_PAGE> rows{};
};
