#include "../include/storage.hpp" 
#include "../include/page.hpp"    
#include "../include/row.hpp"     

#include <algorithm>
#include <iostream>
#include <vector>

using namespace std;
/* ──────────────────────────────────────────────────────────────
   Internal constants (file offsets, sizes)                      */
namespace
{
    constexpr streamoff META_SIZE = sizeof(Storage::Meta);
    constexpr uint64_t PAGE_SIZE_U64 = static_cast<uint64_t>(PAGE_SIZE);
}

/* ──────────────────────────────────────────────────────────────
   Ctor / Dtor                                                   */
Storage::Storage(const string &path) : file_path_(path) { load_or_create(); }

Storage::~Storage()
{
    flush_meta();
    file_.close();
}

/* ──────────────────────────────────────────────────────────────
   Public CRUD                                                   */
bool Storage::insert(const Row &row)
{
    if (index_.count(row.id))
        return false;

    uint64_t offset = allocate_row_slot();
    write_row(offset, row);
    index_[row.id] = offset;
    ++meta_.row_cnt;
    return true;
}

optional<Row> Storage::select(int32_t id) const
{
    auto it = index_.find(id);
    if (it == index_.end())
        return nullopt;
    return read_row(it->second);
}

bool Storage::update(int32_t id, const Row &row)
{
    if (id != row.id)
        return false;
    auto it = index_.find(id);
    if (it == index_.end())
        return false;
    write_row(it->second, row);
    return true;
}

bool Storage::remove(int32_t id)
{
    auto it = index_.find(id);
    if (it == index_.end())
        return false;

    Row tomb{};
    tomb.id = -1;
    tomb.name[0] = '\0';
    tomb.age = 0;

    write_row(it->second, tomb);
    index_.erase(it);
    --meta_.row_cnt;
    return true;
}

/* ──────────────────────────────────────────────────────────────
   Startup / shutdown helpers                                    */
void Storage::load_or_create()
{
    ios::openmode mode = ios::in | ios::out | ios::binary;
    file_.open(file_path_, mode);
    if (!file_)
    {
        mode |= ios::trunc;
        file_.open(file_path_, mode);
        flush_meta();
        return;
    }

    file_.seekg(0);
    file_.read(reinterpret_cast<char *>(&meta_), META_SIZE);
    if (string_view(meta_.magic, 4) != "MYDB")
    {
        file_.seekp(0);
        flush_meta();
        return;
    }

    uint64_t offset = META_SIZE;
    for (uint32_t p = 0; p < meta_.page_cnt; ++p)
    {
        file_.seekg(offset);
        PageHeader ph{};
        file_.read(reinterpret_cast<char *>(&ph), sizeof(ph));

        for (uint16_t i = 0; i < ph.row_count; ++i)
        {
            uint64_t row_off = offset + sizeof(PageHeader) + i * sizeof(Row);
            Row r = read_row(row_off);
            if (r.id >= 0)
                index_[r.id] = row_off;
        }
        offset += PAGE_SIZE_U64;
    }
}

void Storage::flush_meta()
{
    file_.seekp(0);
    file_.write(reinterpret_cast<const char *>(&meta_), META_SIZE);
    file_.flush();
}

/* ──────────────────────────────────────────────────────────────
   Allocate space for a new row                                  */
uint64_t Storage::allocate_row_slot()
{
    uint64_t file_size = static_cast<uint64_t>(file_.seekp(0, ios::end).tellp());
    uint64_t offset;

    if (meta_.page_cnt == 0 || (file_size - META_SIZE) % PAGE_SIZE_U64 == 0)
    {
        uint64_t page_start = file_size;

        PageHeader ph{};
        ph.row_count = 1;
        file_.write(reinterpret_cast<const char *>(&ph), sizeof(ph));

        ++meta_.page_cnt;
        flush_meta();

        offset = page_start + sizeof(PageHeader);
    }
    else
    {
        uint64_t page_start = META_SIZE + (meta_.page_cnt - 1) * PAGE_SIZE_U64;

        file_.seekg(page_start);
        PageHeader ph{};
        file_.read(reinterpret_cast<char *>(&ph), sizeof(ph));

        offset = page_start + sizeof(PageHeader) + ph.row_count * sizeof(Row);

        ++ph.row_count;
        file_.seekp(page_start);
        file_.write(reinterpret_cast<const char *>(&ph), sizeof(ph));
    }

    file_.flush();
    return offset;
}

/* ──────────────────────────────────────────────────────────────
   Low‑level row I/O                                             */
void Storage::write_row(uint64_t offset, const Row &row)
{
    file_.seekp(offset);
    file_.write(reinterpret_cast<const char *>(&row), sizeof(Row));
    file_.flush();
}

Row Storage::read_row(uint64_t offset) const
{
    Row r{};
    file_.seekg(offset);
    file_.read(reinterpret_cast<char *>(&r), sizeof(Row));
    return r;
}

/* ──────────────────────────────────────────────────────────────
   Utility: return all rows (for list & export)                  */
vector<Row> Storage::get_all() const
{
    vector<Row> rows;
    rows.reserve(meta_.row_cnt);

    for (const auto &[id, off] : index_)
        rows.emplace_back(read_row(off));

    sort(rows.begin(), rows.end(),
         [](const Row &a, const Row &b)
         { return a.id < b.id; });

    return rows;
}

vector<Row> Storage::search_by_name(const string &name) const
{
    vector<Row> result;
    for (const auto &[id, offset] : index_)
    {
        Row r = read_row(offset);
        if (r.name == name)
            result.push_back(r);
    }
    return result;
}

vector<Row> Storage::search_by_age(const string &op, int32_t value) const
{
    vector<Row> result;
    for (const auto &[id, offset] : index_)
    {
        Row r = read_row(offset);
        if ((op == "=" && r.age == value) ||
            (op == ">" && r.age > value) ||
            (op == "<" && r.age < value) ||
            (op == ">=" && r.age >= value) ||
            (op == "<=" && r.age <= value))
        {
            result.push_back(r);
        }
    }
    return result;
}