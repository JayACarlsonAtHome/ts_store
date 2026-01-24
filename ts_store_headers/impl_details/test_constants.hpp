// ts_store/ts_store_headers/impl_details/test_constants.hpp
// Created by jay on 1/12/26.
//
#pragma once

#include <array>
#include <string_view>

inline static constexpr std::array<std::string_view, 5> types = {
    "INFO",
    "WARN",
    "ERROR",
    "TRACE",
    "DEBUG"
};

inline static constexpr std::array<std::string_view, 5> categories = {
    "NET",
    "DB",
    "UI",
    "SYS",
    "GFX"
};
inline static constexpr std::array<std::string_view, 5> test_messages = {
    "[INFO]  Processing request",
    "[WARN]  Resource usage high",
    "[ERROR] Connection failed",
    "[INFO]  Cache hit ratio 98%",
    "[DEBUG] Thread pool active"
};



