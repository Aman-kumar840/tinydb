#include "../include/table_storage.hpp"
#include "../include/row.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>
#include <unordered_map>

using namespace std;
// ───────── helpers ──────────────────────────────────────────────────────────
static string schema_path(const string &t) { return t + ".schema"; }
static string data_path(const string &t) { return t + ".db"; }
static string index_path(const string &t) { return t + ".index"; } 

static TableSchema load_schema(const string &table)
{
    ifstream in(schema_path(table), ios::binary);
    if (!in) throw runtime_error("schema file missing");

    TableSchema s;
    s.table_name = table;
    uint16_t ncols;
    in.read(reinterpret_cast<char *>(&ncols), sizeof(ncols));

    for (uint16_t i = 0; i < ncols; ++i) {
        uint16_t len;
        in.read(reinterpret_cast<char *>(&len), sizeof(len));
        string name(len, '\0');
        in.read(name.data(), len);

        Column c;
        c.name = std::move(name);
        in.read(reinterpret_cast<char *>(&c.type), sizeof(c.type));
        in.read(reinterpret_cast<char *>(&c.size), sizeof(c.size));

        s.columns.push_back(c);
        s.row_size += c.size;
    }
    return s;
}

// ───────── constructor ──────────────────────────────────────────────────────
TableStorage::TableStorage(TableSchema s, const string &p, const string &index_p)
    : schema_(std::move(s)), file_path_(p), index_path_(index_p)
{
    file_.open(p, ios::in | ios::out | ios::binary);
    if (!file_) {
        file_.open(p, ios::out | ios::binary);
        file_.close();
        file_.open(p, ios::in | ios::out | ios::binary);
    }
    file_.seekp(0, ios::end);
    file_end_ = file_.tellp();

    max_rows_per_page_ = (PAGE_SIZE - PAGE_HEADER_SIZE) / schema_.row_size;
    if (max_rows_per_page_ == 0) throw runtime_error("Row too large for 4KB page");

    load_index(); 
}

unique_ptr<TableStorage> TableStorage::open(const string &t)
{
    TableSchema s = load_schema(t);
    return unique_ptr<TableStorage>(new TableStorage(s, data_path(t), index_path(t)));
}

// ───────── pack / unpack ─────────────────────────────────────────────────────
vector<uint8_t> TableStorage::pack(const vector<string> &tok) const
{
    if (tok.size() != schema_.columns.size()) throw runtime_error("arity mismatch");
    vector<uint8_t> raw(schema_.row_size);
    size_t off = 0;

    for (size_t i = 0; i < tok.size(); ++i) {
        const auto &col = schema_.columns[i];
        if (col.type == ColType::UINT) {
            uint32_t v = stoul(tok[i]);
            memcpy(raw.data() + off, &v, 4);
        } else if (col.type == ColType::INT) {
            int32_t v = stoi(tok[i]);
            memcpy(raw.data() + off, &v, 4);
        } else { 
            memset(raw.data() + off, 0, col.size);
            strncpy(reinterpret_cast<char *>(raw.data() + off), tok[i].c_str(), col.size - 1);
        }
        off += col.size;
    }
    return raw;
}

vector<string> TableStorage::unpack(const vector<uint8_t> &raw) const
{
    vector<string> out;
    out.reserve(schema_.columns.size());
    size_t off = 0;

    for (const auto &col : schema_.columns) {
        if (col.type == ColType::UINT) {
            uint32_t v;
            memcpy(&v, raw.data() + off, 4);
            out.push_back(to_string(v));
        } else if (col.type == ColType::INT) {
            int32_t v;
            memcpy(&v, raw.data() + off, 4);
            out.push_back(to_string(v));
        } else {
            out.emplace_back(reinterpret_cast<const char *>(raw.data() + off));
        }
        off += col.size;
    }
    return out;
}

// ───────── 4KB Paging Block I/O ──────────────────────────────────────────────
vector<uint8_t> TableStorage::read_page(uint32_t page_id) const
{
    vector<uint8_t> page(PAGE_SIZE, 0);
    uint64_t offset = static_cast<uint64_t>(page_id) * PAGE_SIZE;
    if (offset < file_end_) {
        file_.seekg(offset);
        file_.read(reinterpret_cast<char*>(page.data()), PAGE_SIZE);
    }
    return page;
}

void TableStorage::write_page(uint32_t page_id, const vector<uint8_t>& page_data)
{
    uint64_t offset = static_cast<uint64_t>(page_id) * PAGE_SIZE;
    file_.seekp(offset);
    file_.write(reinterpret_cast<const char*>(page_data.data()), PAGE_SIZE);
    file_.flush();
    if (offset + PAGE_SIZE > file_end_) {
        file_end_ = offset + PAGE_SIZE;
    }
}

// ───────── public API ───────────────────────────────────────────────────────
bool TableStorage::insert_row(const vector<string> &tokens)
{
    if (index_.count(tokens[0])) {
        cerr << "Error: ID already exists.\n";
        return false;
    }

    uint32_t total_pages = file_end_ / PAGE_SIZE;
    uint32_t target_page = total_pages; // default to a brand new page
    uint16_t target_slot = 0;
    bool found_space = false;
    vector<uint8_t> page;

    // Scan existing pages for free space
    for (uint32_t p = 0; p < total_pages; ++p) {
        page = read_page(p);
        uint16_t allocated_slots;
        memcpy(&allocated_slots, page.data(), PAGE_HEADER_SIZE);
        
        if (allocated_slots < max_rows_per_page_) {
            target_page = p;
            target_slot = allocated_slots;
            found_space = true;
            
            // Increment the page header count
            allocated_slots++;
            memcpy(page.data(), &allocated_slots, PAGE_HEADER_SIZE);
            break;
        }
    }

    if (!found_space) {
        // Build a brand new page in RAM
        page = vector<uint8_t>(PAGE_SIZE, 0);
        uint16_t allocated_slots = 1;
        memcpy(page.data(), &allocated_slots, PAGE_HEADER_SIZE);
    }

    // Write the row data exactly where it belongs in the page buffer
    auto raw = pack(tokens);
    size_t local_offset = PAGE_HEADER_SIZE + (target_slot * schema_.row_size);
    memcpy(page.data() + local_offset, raw.data(), schema_.row_size);

    // Blast the 4KB block back to disk
    write_page(target_page, page);

    // Update our index with the absolute global byte offset
    uint64_t global_offset = (static_cast<uint64_t>(target_page) * PAGE_SIZE) + local_offset;
    index_[tokens[0]] = global_offset; 
    save_index();
    return true;
}

vector<vector<string>> TableStorage::select_all() const
{
    vector<vector<string>> rows;
    uint32_t total_pages = file_end_ / PAGE_SIZE;
    
    for (uint32_t p = 0; p < total_pages; ++p) {
        auto page = read_page(p);
        uint16_t allocated_slots;
        memcpy(&allocated_slots, page.data(), PAGE_HEADER_SIZE);
        
        for (uint16_t slot = 0; slot < allocated_slots; ++slot) {
            size_t local_offset = PAGE_HEADER_SIZE + (slot * schema_.row_size);
            vector<uint8_t> raw(page.begin() + local_offset, page.begin() + local_offset + schema_.row_size);
            
            auto tokens = unpack(raw);
            string id = tokens[0];
            if (id == "-1" || id == "__DEL__" || id == "4294967295") continue;
            
            rows.push_back(tokens);
        }
    }
    return rows;
}

bool TableStorage::delete_by_id(const string &id_tok)
{
    auto it = index_.find(id_tok);
    if (it == index_.end()) return false; 
    
    uint64_t global_offset = it->second;
    uint32_t page_id = global_offset / PAGE_SIZE;
    size_t local_offset = global_offset % PAGE_SIZE;

    // Load the 4KB block
    auto page = read_page(page_id);
    
    vector<uint8_t> raw(page.begin() + local_offset, page.begin() + local_offset + schema_.row_size);
    auto tokens = unpack(raw);
    
    if (schema_.columns[0].type == ColType::VARCHAR) {
        tokens[0] = "__DEL__";
    } else {
        tokens[0] = "-1"; 
    }

    // Overwrite locally and save block
    auto modified_raw = pack(tokens);
    memcpy(page.data() + local_offset, modified_raw.data(), schema_.row_size);
    write_page(page_id, page);

    index_.erase(it);
    save_index();
    return true;
}

bool TableStorage::update_by_id(const string &id_tok, const vector<pair<string, string>> &changes)
{
    auto it = index_.find(id_tok);
    if (it == index_.end()) return false;
    
    uint64_t global_offset = it->second;
    uint32_t page_id = global_offset / PAGE_SIZE;
    size_t local_offset = global_offset % PAGE_SIZE;

    // Load the 4KB block
    auto page = read_page(page_id);

    vector<uint8_t> raw(page.begin() + local_offset, page.begin() + local_offset + schema_.row_size);
    auto tokens = unpack(raw);

    for (const auto &[col_name, new_val] : changes) {
        for (size_t i = 0; i < schema_.columns.size(); ++i) {
            if (schema_.columns[i].name == col_name) {
                tokens[i] = new_val;
            }
        }
    }

    // Overwrite locally and save block
    auto modified_raw = pack(tokens);
    memcpy(page.data() + local_offset, modified_raw.data(), schema_.row_size);
    write_page(page_id, page);

    if (tokens[0] != id_tok) {
        index_.erase(id_tok);
        index_[tokens[0]] = global_offset;
    }

    save_index();
    return true;
}

bool TableStorage::export_csv(const string &path) const
{
    ofstream out(path);
    if (!out) return false;

    for (size_t i = 0; i < schema_.columns.size(); ++i)
        out << schema_.columns[i].name << (i + 1 == schema_.columns.size() ? '\n' : '\t');

    for (const auto &r : select_all()) {
        for (size_t i = 0; i < r.size(); ++i)
            out << r[i] << (i + 1 == r.size() ? '\n' : '\t');
    }
    return true;
}

// ───────── index management ─────────────────────────────────────────────────
void TableStorage::save_index() const
{
    ofstream out(index_path_, ios::binary | ios::trunc);
    if (!out) return;

    size_t count = index_.size();
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& [id, offset] : index_) {
        size_t id_len = id.size();
        out.write(reinterpret_cast<const char*>(&id_len), sizeof(id_len));
        out.write(id.data(), id_len);
        out.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
    }
}

void TableStorage::load_index()
{
    ifstream in(index_path_, ios::binary);
    if (!in) return; 

    size_t count;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    for (size_t i = 0; i < count; ++i) {
        size_t id_len;
        in.read(reinterpret_cast<char*>(&id_len), sizeof(id_len));
        string id(id_len, '\0');
        in.read(id.data(), id_len);
        uint64_t offset;
        in.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        
        index_[id] = offset;
    }
}