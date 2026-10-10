#pragma once

#include <format>
#include <iostream>
#include <utility>

template <typename... Args> void print(std::format_string<Args...> pattern, Args&&... args) {
    std::cout << std::format(pattern, std::forward<Args>(args)...);
}

template <typename... Args> void println(std::format_string<Args...> pattern, Args&&... args) {
    std::cout << std::format(pattern, std::forward<Args>(args)...) << '\n';
}
