#include "storage.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <vector>

int main()
{
    Storage db("mydata.db");

    std::cout << "TinyDB REPL — commands: insert|select|update|delete|exit\n";
    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line))
    {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        if (cmd == "insert")
        {
            int id, age;
            std::string name;
            iss >> id >> std::quoted(name) >> age;
            if (db.insert(Row{id, name, age}))
                std::cout << "OK\n";
            else
                std::cout << "Duplicate id!\n";
        }

        else if (cmd == "select")
        {
            int id;
            iss >> id;
            auto r = db.select(id);
            if (r)
                std::cout << r->id << " \"" << r->name << "\" " << r->age << '\n';
            else
                std::cout << "Not found\n";
        }
        else if (cmd == "update")
        {
            int id, age;
            std::string name;
            iss >> id >> std::quoted(name) >> age;
            std::cout << (db.update(id, Row{id, name, age}) ? "Updated\n" : "Missing id\n");
        }
        else if (cmd == "delete")
        {
            int id;
            iss >> id;
            std::cout << (db.remove(id) ? "Deleted\n" : "Missing id\n");
        }
        else if (cmd == "exit")
        {
            break;
        }
        else if (cmd == "delete")
        {
            int id;
            iss >> id;
            std::cout << (db.remove(id) ? "Deleted\n" : "Missing id\n");

            // ---------- NEW COMMAND: list ----------
        }
        else if (cmd == "list")
        {
            auto rows = db.get_all();
            for (const auto &r : rows)
                std::cout << r.id << " \"" << r.name << "\" " << r.age << '\n';
            std::cout << rows.size() << " row(s)\n";

            // ---------- NEW COMMAND: export json ----------
        }
        else if (cmd == "export")
        {
            std::string fmt;
            iss >> fmt;
            if (fmt == "json")
            {
                auto rows = db.get_all();
                std::ofstream out("data.json");
                out << "[\n";
                for (std::size_t i = 0; i < rows.size(); ++i)
                {
                    const auto &r = rows[i];
                    out << "  {\"id\":" << r.id
                        << ",\"name\":\"" << r.name
                        << "\",\"age\":" << r.age << "}";
                    if (i + 1 != rows.size())
                        out << ',';
                    out << '\n';
                }
                out << "]\n";
                std::cout << "Exported " << rows.size() << " row(s) to data.json\n";
            }
            else
            {
                std::cout << "Unknown export format\n";
            }
        }
    }
}