cmake_minimum_required(VERSION 3.26)
project(ts_store LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options for portability
option(ENABLE_NATIVE "Enable -march=native" OFF)
option(ENABLE_LTO "Enable Link Time Optimization" ON)
option(ENABLE_CUSTOM_LLVM "Use custom LLVM/libc++ paths" OFF)

# INTERFACE for warnings (inspired by Jason Turner's style)
add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Werror -Wconversion
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
        -Wunused -Woverloaded-virtual -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion -Wformat=2
        $<$<CXX_COMPILER_ID:GNU>:-Wno-interference-size>
)

# INTERFACE for release optimizations
add_library(project_options INTERFACE)
if(ENABLE_LTO)
    target_compile_options(project_options INTERFACE
            $<$<CONFIG:Release>:-flto=auto -ffat-lto-objects>
    )
    target_link_options(project_options INTERFACE
            $<$<CONFIG:Release>:-flto>
    )
endif()
if(ENABLE_NATIVE)
    target_compile_options(project_options INTERFACE
            $<$<CONFIG:Release>:-O3 -march=native>
    )
else()
    target_compile_options(project_options INTERFACE
            $<$<CONFIG:Release>:-O3>
    )
endif()

# Custom Clang/libc++ setup (only when requested)
if(ENABLE_CUSTOM_LLVM)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        if(ENABLE_LTO)
            target_compile_options(project_options INTERFACE
                    $$   <   $$<CONFIG:Release>:$$   <   $$<CXX_COMPILER_ID:Clang>:-flto=thin>$$   <   $$<CXX_COMPILER_ID:GNU>:-flto=auto> -ffat-lto-objects>
            )
            target_link_options(project_options INTERFACE
                    $$   <   $$<CONFIG:Release>:$$   <   $$<CXX_COMPILER_ID:Clang>:-flto=thin>$$   <   $$<CXX_COMPILER_ID:GNU>:-flto>>
            )
            if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
                target_link_options(project_options INTERFACE
                        $$   <   $$<CONFIG:Release>:-fuse-ld=lld>
                )
                message(STATUS "Clang + LTO detected — using ThinLTO and lld linker")
            endif()
        endif()

        target_include_directories(project_options SYSTEM INTERFACE
                /home/jay/Tools/llvm-full_release_22_0_0/include/c++/v1
                /home/jay/Tools/llvm-full_release_22_0/lib/clang/22/include
        )
        target_link_directories(project_options INTERFACE
                /home/jay/Tools/llvm-full_release_22_0/lib
        )
    endif()
endif()

# Disable sanitizers globally (keep your preference)
add_compile_options(-fno-sanitize=undefined -fno-sanitize=all)
add_link_options(-fno-sanitize=undefined -fno-sanitize=all)

set(ts_flags_source "ts_store_flags/Test_Flags.cpp")
set(ts_flags_target "ts_store_flags")
target_compile_features(${ts_flags_target} PRIVATE cxx_std_23)
target_include_directories(${ts_flags_target} PRIVATE ../ts_store_headers)
target_link_libraries(${ts_flags_target} PRIVATE project_warnings project_options)
add_executable(${ts_flags_target} ${ts_flags_source})

set(TEST_NUMBERS 001 002 003 004 005)

foreach(num IN LISTS TEST_NUMBERS)
    # TS version (timestamps ON)
    set(ts_source "ts_store_${num}/test_${num}_TS.cpp")
    set(ts_target "ts_store_${num}_TS")
    add_executable(${ts_target} ${ts_source})
    target_include_directories(${ts_target} PRIVATE ../ts_store_headers)
    target_compile_definitions(${ts_target} PRIVATE
            TS_STORE_ENABLE_TEST_CHECKS
            USE_TIMESTAMPS=1
    )
    target_link_libraries(${ts_target} PRIVATE project_warnings project_options)

    # XS version (timestamps OFF)
    set(xs_source "ts_store_${num}/test_${num}_XS.cpp")
    set(xs_target "ts_store_${num}_XS")
    add_executable(${xs_target} ${xs_source})
    target_include_directories(${xs_target} PRIVATE ../ts_store_headers)
    target_compile_definitions(${xs_target} PRIVATE
            TS_STORE_ENABLE_TEST_CHECKS
            USE_TIMESTAMPS=0
    )
    target_link_libraries(${xs_target} PRIVATE project_warnings project_options)
endforeach()
