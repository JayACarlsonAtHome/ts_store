cmake_minimum_required(VERSION 3.26)
project(ts_store LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Max performance
add_compile_options(-O3 -march=native -flto=auto -ffat-lto-objects)
add_link_options(-flto=auto)

set(TEST_NUMBERS 001 002 003 004 005 )

foreach(num IN LISTS TEST_NUMBERS)

    # TS version (timestamps ON)
    set(source_file "ts_store_${num}/test_${num}_TS.cpp")
    set(ts_target "ts_store_${num}_TS")
    add_executable(${ts_target} ${source_file} fmt/src/format.cc fmt/src/os.cc)
    target_include_directories(${ts_target} PRIVATE ../ts_store_headers fmt/include)
    target_compile_definitions(${ts_target} PRIVATE TS_STORE_ENABLE_TEST_CHECKS USE_TIMESTAMPS=1)
    target_compile_options(${ts_target} PRIVATE -Werror -Wall -Wextra -Wpedantic -Wno-interference-size)

    # XS version (No TimeStamps)
    set(source_file "ts_store_${num}/test_${num}_XS.cpp")
    set(ts_target "ts_store_${num}_XS")
    add_executable(${ts_target} ${source_file} fmt/src/format.cc fmt/src/os.cc)
    target_include_directories(${ts_target} PRIVATE ../ts_store_headers fmt/include)

    target_compile_definitions(${ts_target} PRIVATE TS_STORE_ENABLE_TEST_CHECKS USE_TIMESTAMPS=0)
    target_compile_options(${ts_target} PRIVATE -Werror -Wall -Wextra -Wpedantic -Wno-interference-size)
endforeach()
