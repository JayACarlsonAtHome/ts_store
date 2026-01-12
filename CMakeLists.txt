cmake_minimum_required(VERSION 3.26)
project(ts_store LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)



# Global performance flags (apply to all targets)
add_compile_options(-O3 -march=native -flto=auto -ffat-lto-objects)
add_link_options(-flto)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-nostdinc++ -nostdinc)
    include_directories(SYSTEM /home/jay/Tools/llvm-full_release_22_0_0/include/c++/v1)
    include_directories(SYSTEM /home/jay/Tools/llvm-full_release_22_0_0/lib/clang/22/include)
    link_directories(/home/jay/Tools/llvm-full_release_22_0_0/lib)
endif()

set(TEST_NUMBERS 001 002 003 004 005)

foreach(num IN LISTS TEST_NUMBERS)

    # TS version (timestamps ON)
    set(source_file "ts_store_${num}/test_${num}_TS.cpp")
    set(ts_target "ts_store_${num}_TS")
    add_executable(${ts_target} ${source_file})
    target_include_directories(${ts_target} PRIVATE ../ts_store_headers)
    target_compile_definitions(${ts_target} PRIVATE TS_STORE_ENABLE_TEST_CHECKS USE_TIMESTAMPS=1)

    # XS version (No TimeStamps)
    set(source_file "ts_store_${num}/test_${num}_XS.cpp")
    set(xs_target "ts_store_${num}_XS")
    add_executable(${xs_target} ${source_file})
    target_include_directories(${xs_target} PRIVATE ../ts_store_headers)
    target_compile_definitions(${xs_target} PRIVATE TS_STORE_ENABLE_TEST_CHECKS USE_TIMESTAMPS=0)

    # Common compile options for both targets
    foreach(target ${ts_target} ${xs_target})
       
        target_compile_options(${target} PRIVATE
            -Werror -Wall -Wextra -Wpedantic
            -fno-sanitize=undefined -fno-sanitize=all
            $<$<CXX_COMPILER_ID:GNU>:-Wno-interference-size>
        )
        
        target_link_options(${target} PRIVATE
            -fno-sanitize=undefined -fno-sanitize=all
            $<$<CXX_COMPILER_ID:Clang>:-stdlib=libc++>
        )
    endforeach()
endforeach()
