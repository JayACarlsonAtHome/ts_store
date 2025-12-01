cmake_minimum_required(VERSION 3.26)
project(ts_store LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Uncomment when ready for max performance
add_compile_options(-O3 -march=native -flto --param destructive-interference-size=64)

# ------------------------------------------------------------------
# The One True Order — clean, sequential, obvious
# ------------------------------------------------------------------
set(TEST_NUMBERS
        001
        002
        003
        004
        005
)

foreach(num IN LISTS TEST_NUMBERS)
    set(target_name "ts_store_${num}")

    add_executable(${target_name}
            ${target_name}/test_${num}.cpp
    )

    target_include_directories(${target_name} PRIVATE
            ../ts_store_headers
    )

    target_compile_definitions(${target_name} PRIVATE
            TS_STORE_ENABLE_TEST_CHECKS
    )

    target_compile_options(${target_name} PRIVATE
            -Werror -Wall -Wextra -Wpedantic
            -Wno-interference-size          # ← fixes GTL warning
    )
endforeach()
