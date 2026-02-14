// ts_store/ts_store_headers/impl_details/test_constants.hpp
// Created by jay on 1/12/26.
//
#pragma once

#include <array>
#include <string_view>

inline static constexpr std::array<std::string_view, 5> categories = {
    "NET",
    "DB",
    "UI",
    "SYS",
    "GFX"
};

inline static constexpr std::array<std::string_view, 8> test_messages = {
    "Not Set, default payload",
    "Trace processing request",
    "Debug processing request",
    "Info processing request",
    "Warning in processing notifiation",
    "Error in processing notification",
    "Critical processing request",
    "Fatal error in processing notification",

};



