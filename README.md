# TinyDB

A high-performance, dynamic relational database engine built entirely from scratch in under 1,000 lines of modern C++.

TinyDB implements industrial-grade backend architecture, including a custom 4KB block-paging storage system, persistent hash indexing, and true $O(1)$ memory mapping to deliver sub-millisecond execution times. It features an interactive REPL capable of parsing SQL-style commands to execute schema creation, inserts, updates, deletions, and data exports.

---

## 🚀 Core Architecture

- **4KB Block-Paging Storage:** Reads and writes binary data in fixed 4096-byte pages (mirroring OS-level disk blocks) to maximize I/O throughput and efficiently pack variable-length records.

- **Persistent Hash Indexing:** Maintains a serialized `.index` mapping file, bypassing full-table scans to achieve true $O(1)$ direct-access lookups on startup across 50K+ records.

- **Tombstone Deletions & In-Place Updates:** Resolves $O(N)$ file-rewrite bottlenecks by executing lazy deletions and exact-byte memory overwrites for optimal CRUD latency.

- **Dynamic Schema Parsing:** Generates custom binary packing/unpacking routines on the fly based on user-defined table structures.

---

## 🛠️ Quick Start

### Build the Engine

```bash
make
```

### Launch the REPL

```bash
./bin/tinydb_cli
```

---

## Supported SQL-Style Operations

### 1. Create a Table

```sql
create table users ( id int, name varchar(32), age int )
```

### 2. Insert Data

```sql
insert into users values ( 1, "Alice", 25 )
insert into users values ( 2, "Bob", 30 )
```

### 3. Read Data

```sql
select * from users
```

### 4. Update Data (In-place binary update)

```sql
update users set age = 99 where id = 1
```

### 5. Delete Data (Tombstone deletion)

```sql
delete from users where id = 2
```

### 6. Export to CSV

```sql
export csv users output.csv
```

---

## 📂 File System Structure

When a table is created, TinyDB generates three tightly coupled binary files:

- `table_name.schema`: Stores the column definitions and byte-arity of the table.
- `table_name.db`: The core 4KB block-paged data file.
- `table_name.index`: The serialized map of ID primary keys to absolute global byte offsets.

---

## 🧠 Concepts Demonstrated

- Custom Database Engine Design
- Block-Paged Storage Architecture
- Persistent Hash Indexing
- Binary File Handling
- Dynamic Schema Parsing
- Memory Mapping Concepts
- CRUD Operations
- SQL-style Query Parsing
- Storage Optimization
- File System Design

---

## ⭐ Future Improvements

- Add B+ Tree indexing support
- Introduce transaction management (ACID properties)
- Implement concurrency control and locking
- Add support for JOIN operations
- Support secondary indexes and query optimization
- Build a TCP server for client connections

---

# 👨‍💻 Author

## Aman Kumar

- GitHub: https://github.com/Aman-kumar840
- LinkedIn: https://www.linkedin.com/in/aman-kumar-016927308/

If you found this project useful, consider giving it a ⭐ on GitHub.