cmake_minimum_required(VERSION 3.26)
project(ts_store LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(--param destructive-interference-size=64)

add_executable(ts_store_base          ts_store_base/ts_store_base.cpp)
add_executable(ts_store_concurrent    ts_store_concurrent/ts_store_concurrent.cpp)
add_executable(ts_store_stress        ts_store_stress/ts_store_stress.cpp)
add_executable(ts_store_big_test      ts_store_big_test/ts_store_big_test.cpp)
add_executable(final_massive_test     ts_store_final_massive_test/final_massive_test.cpp)

# All executables need the folder that contains ts_store.hpp and GTL
target_include_directories(ts_store_base          PRIVATE ../ts_store_headers)
target_include_directories(ts_store_concurrent    PRIVATE ../ts_store_headers)
target_include_directories(ts_store_stress        PRIVATE ../ts_store_headers)
target_include_directories(ts_store_big_test      PRIVATE ../ts_store_headers)
target_include_directories(final_massive_test     PRIVATE ../ts_store_headers)

# Optional: if you still get GTL warnings on some platforms
# target_compile_options(ts_store_base PRIVATE -Wno-interference-size)