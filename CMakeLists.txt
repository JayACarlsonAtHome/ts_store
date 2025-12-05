cmake_minimum_required(VERSION 3.26)
project(ts_store LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# THIS LINE FIXES THE GCC 15 + <ranges> + fmt BUG
add_compile_options(-Wno-error=template-body)

# Max performance
add_compile_options(-O3 -march=native -flto=auto -ffat-lto-objects)
add_link_options(-flto=auto)

set(TEST_NUMBERS 001 002 003 004 005)

foreach(num IN LISTS TEST_NUMBERS)
    set(target_name "ts_store_${num}")

    add_executable(${target_name}
            ${target_name}/test_${num}.cpp
            fmt/src/format.cc
            fmt/src/os.cc
    )

    target_include_directories(${target_name} PRIVATE
            ../ts_store_headers
            fmt/include
    )

    target_compile_definitions(${target_name} PRIVATE
            TS_STORE_ENABLE_TEST_CHECKS
    )

    target_compile_options(${target_name} PRIVATE
            -Werror -Wall -Wextra -Wpedantic
            -Wno-interference-size
    )
endforeach()