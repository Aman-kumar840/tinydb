#pragma once
#include "row.hpp"
#include "page.hpp"

#include <fstream>
#include <unordered_map>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>
using namespace std;

class Storage
{
public:
    struct Meta
    {
        char magic[4]{'M', 'Y', 'D', 'B'}; // file signature
        uint32_t page_cnt{0};              // how many pages in file
        uint32_t row_cnt{0};               // active (non-deleted) rows
    };

    explicit Storage(const string &path);
    ~Storage();

    bool insert(const Row &row);
    optional<Row> select(int32_t id) const;
    bool update(int32_t id, const Row &row);
    bool remove(int32_t id);

    vector<Row> get_all() const;
    vector<Row> search_by_name(const string &name) const;
    vector<Row> search_by_age(const string &op, int32_t value) const;

private:
    void load_or_create();
    void flush_meta();
    uint64_t allocate_row_slot();

    void write_row(uint64_t offset, const Row &row);
    Row read_row(uint64_t offset) const;

    string file_path_;
    mutable fstream file_;
    Meta meta_;
    unordered_map<int32_t, uint64_t> index_; // id → file offset
};