cmake_minimum_required(VERSION 3.26)
project(ts_store LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Werror -Wconversion
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
        -Wunused -Woverloaded-virtual -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion -Wformat=2
        -Wundef  -Wsuggest-override
        $<$<CXX_COMPILER_ID:GNU>:-Wno-interference-size>
)

add_library(project_options INTERFACE)
target_compile_options(project_options INTERFACE
          $<$<CONFIG:Release>:-O3>
)
option(ENABLE_CUSTOM_LLVM "Use custom LLVM/libc++ paths" OFF)

if(ENABLE_CUSTOM_LLVM)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(project_options INTERFACE
                $$   <   $$<CONFIG:Release>:-flto=thin>
        )
        target_link_options(project_options INTERFACE
                $$   <   $$<CONFIG:Release>:-flto=thin>
        )
        target_include_directories(project_options INTERFACE
                /home/jay/Tools/llvm-full_release_22_0/lib/clang/22/include
        )
        target_link_directories(project_options INTERFACE
                /home/jay/Tools/llvm-full_release_22_0/lib
        )
    endif()
endif()


# Flags test
add_executable(ts_store_flags tests/ts_store_flags/Test_Flags.cpp)
target_include_directories(ts_store_flags PRIVATE ../ts_store_headers)
target_link_libraries(ts_store_flags PRIVATE project_warnings project_options)

set(TEST_NUMBERS 001 002 003 004 005 006 007)

foreach(num IN LISTS TEST_NUMBERS)
    # TS version
    add_executable(ts_store_${num}_TS tests/ts_store_${num}/test_${num}_TS.cpp)
    target_include_directories(ts_store_${num}_TS PRIVATE ../ts_store_headers)
    target_compile_definitions(ts_store_${num}_TS PRIVATE TS_STORE_ENABLE_TEST_CHECKS USE_TIMESTAMPS=1)
    target_link_libraries(ts_store_${num}_TS PRIVATE project_warnings project_options)

    # XS version
    add_executable(ts_store_${num}_XS tests/ts_store_${num}/test_${num}_XS.cpp)
    target_include_directories(ts_store_${num}_XS PRIVATE ../ts_store_headers)
    target_compile_definitions(ts_store_${num}_XS PRIVATE TS_STORE_ENABLE_TEST_CHECKS USE_TIMESTAMPS=0)
    target_link_libraries(ts_store_${num}_XS PRIVATE project_warnings project_options)
endforeach()