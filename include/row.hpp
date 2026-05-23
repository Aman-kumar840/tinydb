#pragma once
#include <cstdint>
#include <cstring>
#include <string_view>

struct Row
{
    int32_t id{};
    char name[32]{};
    int32_t age{};

    Row() = default;

    Row(int32_t i, std::string_view n, int32_t a) : id(i), age(a)
    {
        std::memset(name, 0, sizeof(name));
        std::strncpy(name, n.data(), sizeof(name) - 1);
    }
};