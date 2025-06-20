#include "storage.hpp"
#include <algorithm>
#include <iostream>
#include <vector>

/* ──────────────────────────────────────────────────────────────
   Internal constants (file offsets, sizes)                      */
namespace
{
    constexpr std::streamoff META_SIZE = sizeof(Storage::Meta);
    constexpr uint64_t PAGE_SIZE_U64 = static_cast<uint64_t>(PAGE_SIZE);
}

/* ──────────────────────────────────────────────────────────────
   Ctor / Dtor                                                   */
Storage::Storage(const std::string &path) : file_path_(path) { load_or_create(); }

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
        return false; // duplicate PK

    uint64_t offset = allocate_row_slot();
    write_row(offset, row);
    index_[row.id] = offset;
    ++meta_.row_cnt;
    return true;
}

std::optional<Row> Storage::select(int32_t id) const
{
    auto it = index_.find(id);
    if (it == index_.end())
        return std::nullopt;
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
    tomb.id = -1; // sentinel
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
    std::ios::openmode mode = std::ios::in | std::ios::out | std::ios::binary;
    file_.open(file_path_, mode);
    if (!file_)
    { // first run → create new file
        mode |= std::ios::trunc;
        file_.open(file_path_, mode);
        flush_meta();
        return;
    }

    file_.seekg(0);
    file_.read(reinterpret_cast<char *>(&meta_), META_SIZE);
    if (std::string_view(meta_.magic, 4) != "MYDB")
    { // corrupt/empty file → reset
        file_.seekp(0);
        flush_meta();
        return;
    }

    /* rebuild in‑memory index */
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
                index_[r.id] = row_off; // skip tombstones
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
    uint64_t file_size = static_cast<uint64_t>(file_.seekp(0, std::ios::end).tellp());
    uint64_t offset;

    /* CASE A: need a brand‑new page (no pages yet OR last page full) */
    if (meta_.page_cnt == 0 || (file_size - META_SIZE) % PAGE_SIZE_U64 == 0)
    {
        uint64_t page_start = file_size;

        PageHeader ph{};
        ph.row_count = 1; // reserve slot 0
        file_.write(reinterpret_cast<const char *>(&ph), sizeof(ph));

        ++meta_.page_cnt;
        flush_meta();

        offset = page_start + sizeof(PageHeader); // first row
    }
    /* CASE B: append into the existing last page */
    else
    {
        uint64_t page_start = META_SIZE + (meta_.page_cnt - 1) * PAGE_SIZE_U64;

        file_.seekg(page_start);
        PageHeader ph{};
        file_.read(reinterpret_cast<char *>(&ph), sizeof(ph));

        offset = page_start + sizeof(PageHeader) + ph.row_count * sizeof(Row);

        ++ph.row_count; // update header
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
std::vector<Row> Storage::get_all() const
{
    std::vector<Row> rows;
    rows.reserve(meta_.row_cnt);

    for (const auto &[id, off] : index_)
        rows.emplace_back(read_row(off));

    std::sort(rows.begin(), rows.end(),
              [](const Row &a, const Row &b)
              { return a.id < b.id; });

    return rows;
}
