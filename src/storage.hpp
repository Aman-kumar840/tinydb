#pragma once
#include "page.hpp"
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

class Storage
{
public:
    struct Meta
    { // (already moved here earlier)
        char magic[4] = {'M', 'Y', 'D', 'B'};
        uint32_t page_cnt = 0;
        uint32_t row_cnt = 0;
    };

    explicit Storage(const std::string &path);
    ~Storage();

    bool insert(const Row &row);
    std::optional<Row> select(int32_t id) const;
    bool update(int32_t id, const Row &row);
    bool remove(int32_t id);

    // ---------- NEW ----------
    std::vector<Row> get_all() const; // used by list / export
    // --------------------------

private:
    std::string file_path_;
    mutable std::fstream file_;
    Meta meta_{};
    std::unordered_map<int32_t, uint64_t> index_;

    void load_or_create();
    void flush_meta();
    uint64_t allocate_row_slot();
    void write_row(uint64_t offset, const Row &r);
    Row read_row(uint64_t offset) const;
};
