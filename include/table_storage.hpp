#pragma once
#include "schema.hpp"
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

using namespace std;
class TableStorage
{
public:
    static unique_ptr<TableStorage> open(const string &table);

    bool insert_row(const vector<string> &tokens);
    vector<vector<string>> select_all() const;
    bool delete_by_id(const string &id_token);
    bool update_by_id(const string &id_token,
                      const vector<pair<string, string>> &col_updates);
    bool export_csv(const string &path) const;

    const TableSchema &schema() const { return schema_; }

private:
    explicit TableStorage(TableSchema s, const string &data_path, const string &index_path);

    vector<string> unpack(const vector<uint8_t> &raw) const;
    vector<uint8_t> pack(const vector<string> &tokens) const;

    void load_index();
    void save_index() const;

    // ─── 4KB Paging Architecture ───
    static constexpr size_t PAGE_SIZE = 4096;
    static constexpr size_t PAGE_HEADER_SIZE = sizeof(uint16_t);
    size_t max_rows_per_page_{0}; 

    vector<uint8_t> read_page(uint32_t page_id) const;
    void write_page(uint32_t page_id, const vector<uint8_t>& page_data);

private:
    TableSchema schema_;
    string file_path_; 
    string index_path_; 
    mutable fstream file_;
    uint64_t file_end_{0};
    
    unordered_map<string, uint64_t> index_; 
};