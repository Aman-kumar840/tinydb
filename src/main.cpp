// -------------- TinyDB CLI --------------------------------------------------
// main.cpp
// ---------------------------------------------------------------------------

#include "../include/table_storage.hpp"             
#include "../include/schema.hpp"                    
bool create_table_from_command(const string &line); 

#include <iostream>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <cctype>
using namespace std;
// ──────────────────────────────────────────────────────────────
// Helpers
// ──────────────────────────────────────────────────────────────
static string trim(string s)
{
    while (!s.empty() && isspace(static_cast<unsigned char>(s.front())))
        s.erase(s.begin());
    while (!s.empty() && isspace(static_cast<unsigned char>(s.back())))
        s.pop_back();
    return s;
}
static string to_upper(string s)
{
    for (char &c : s)
        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    return s;
}

// ──────────────────────────────────────────────────────────────
// Global table cache
// ──────────────────────────────────────────────────────────────
static unordered_map<string, unique_ptr<TableStorage>> g_tables;

static TableStorage *ensure_table(const string &name)
{
    string key = to_upper(name);
    if (!g_tables.count(key))
        g_tables[key] = TableStorage::open(key);
    return g_tables[key].get();
}

// ──────────────────────────────────────────────────────────────
// Main REPL
// ──────────────────────────────────────────────────────────────
int main()
{
    cout << "TinyDB REPL — commands:\n"
            "  create table <NAME> ( col type, col type, ... )\n"
            "  insert into <NAME> values ( val, val, ... )\n"
            "  select * from <NAME>\n"
            "  export csv <NAME> <file.csv>\n"
            "  exit\n";

    string line;
    while (cout << "> " && getline(cin, line))
    {
        // ─── NEW: Destroy invisible carriage returns from copy-pasting ───
        if (!line.empty() && line.back() == '\r') line.pop_back();

        istringstream iss(line);
        string cmd;
        iss >> cmd;
        if (cmd.empty())
            continue;

        // ---------- CREATE TABLE ------------------------------------------
        if (cmd == "create")
        {
            if (!create_table_from_command(line))
                cout << "Table creation failed.\n";
            else
                cout << "Table created successfully.\n";
            continue;
        }

        // ---------- INSERT INTO -------------------------------------------
        if (cmd == "insert")
        {
            string into_kw, table, values_kw;
            iss >> into_kw >> table >> values_kw;
            if (into_kw != "into" || values_kw != "values")
            {
                cout << "Syntax: insert into <TABLE> values (...)\n";
                continue;
            }

            string rest;
            getline(iss, rest); 
            size_t open = rest.find('('), close = rest.find(')');
            if (open == string::npos || close == string::npos || open > close)
            {
                cout << "Invalid syntax.\n";
                continue;
            }
            string inside = rest.substr(open + 1, close - open - 1);

            istringstream vss(inside);
            vector<string> tokens;
            string token;
            while (getline(vss, token, ','))
            {
                token = trim(token);
                if (token.size() >= 2 && token.front() == '"' && token.back() == '"')
                    token = token.substr(1, token.size() - 2); 
                tokens.push_back(token);
            }

            if (ensure_table(table)->insert_row(tokens))
                cout << "OK\n";
            else
                cout << "Insert failed.\n";
            continue;
        }

        // ---------- SELECT * FROM ----------------------------------------
        if (cmd == "select")
        {
            string star, from_kw, table;
            iss >> star >> from_kw >> table;
            if (star != "*" || from_kw != "from")
            {
                cout << "Syntax: select * from <TABLE>\n";
                continue;
            }

            auto rows = ensure_table(table)->select_all();
            for (const auto &r : rows)
            {
                for (size_t i = 0; i < r.size(); ++i)
                    cout << r[i] << (i + 1 == r.size() ? '\n' : ',');
            }
            cout << rows.size() << " row(s)\n";
            continue;
        }

        // ---------- EXPORT CSV -------------------------------------------
        if (cmd == "export")
        {
            string fmt, table, file;
            iss >> fmt >> table >> file;
            if (fmt != "csv")
            {
                cout << "Only csv supported.\n";
                continue;
            }
            if (ensure_table(table)->export_csv(file))
                cout << "Exported → " << file << '\n';
            else
                cout << "Export failed.\n";
            continue;
        }
        // ---------- IMPORT CSV ----------------------------------------------------
        // ---------- DELETE --------------------------------------------------------
        if (cmd == "delete")
        {
            string from_kw, table, where_kw, id_col, eq, id_val;
            iss >> from_kw >> table >> where_kw >> id_col >> eq >> id_val;
            if (from_kw != "from" || where_kw != "where" || eq != "=" || id_col != "id")
            {
                cout << "Syntax: delete from <TABLE> where id = <value>\n";
                continue;
            }
            if (ensure_table(table)->delete_by_id(id_val))
                cout << "Deleted.\n";
            else
                cout << "No matching id.\n";
            continue;
        }

        // ---------- UPDATE --------------------------------------------------------
        if (cmd == "update")
        {
            string table, set_kw;
            iss >> table >> set_kw;
            if (set_kw != "set")
            {
                cout << "Syntax: update <TABLE> set col=val [,col=val] where id = <value>\n";
                continue;
            }
            string rest;
            getline(iss, rest);
            auto where_pos = rest.find(" where id = ");
            if (where_pos == string::npos)
            {
                cout << "Missing 'where id ='\n";
                continue;
            }
            string assignments = rest.substr(0, where_pos);
            string id_val = trim(rest.substr(where_pos + 12)); 

            vector<pair<string, string>> changes;
            istringstream ass(assignments);
            string pair;
            while (getline(ass, pair, ','))
            {
                auto eqp = pair.find('=');
                if (eqp == string::npos)
                    continue;
                string col = trim(pair.substr(0, eqp));
                string val = trim(pair.substr(eqp + 1));
                if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
                    val = val.substr(1, val.size() - 2);
                changes.emplace_back(col, val);
            }
            if (ensure_table(table)->update_by_id(id_val, changes))
                cout << "Updated.\n";
            else
                cout << "No matching id.\n";
            continue;
        }

        else if (cmd == "import")
        {
            string fmt, table, file;
            iss >> fmt >> table >> file;
            if (fmt != "csv")
            {
                cout << "Only csv supported.\n";
                continue;
            }

            ifstream in(file);
            if (!in)
            {
                cout << "Cannot open " << file << '\n';
                continue;
            }

            string lineCSV;
            getline(in, lineCSV);

            auto db = ensure_table(table);
            size_t inserted = 0;

            while (getline(in, lineCSV))
            {
                istringstream lss(lineCSV);
                vector<string> tokens;
                string cell;
                while (getline(lss, cell, '\t'))
                    tokens.push_back(trim(cell));
                if (db->insert_row(tokens))
                    ++inserted;
            }
            cout << "Imported " << inserted << " row(s) from " << file << '\n';
            continue;
        }

        // ---------- EXIT --------------------------------------------------
        if (cmd == "exit")
            break;

        cout << "Unknown command!\n";
    }
    return 0;
}