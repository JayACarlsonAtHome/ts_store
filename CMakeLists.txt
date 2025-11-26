cmake_minimum_required(VERSION 3.26)

project(ts_store)

set(CMAKE_CXX_STANDARD 17)

# Needed for Compiler not to complain about GTL
#g++ Pin the interference size to 64—bye, warnings
add_compile_options(--param destructive-interference-size=64)
#The above line is not needed in RHEL

add_executable(ts_store_base          ts_store_base/ts_store_base.cpp                      ts_store.hpp)
add_executable(ts_store_concurrent    ts_store_concurrent/ts_store_concurrent.cpp          ts_store.hpp)
add_executable(ts_store_stress        ts_store_stress/ts_store_stress.cpp                  ts_store.hpp)
add_executable(ts_store_big_test      ts_store_big_test/ts_store_big_test.cpp              ts_store.hpp)
add_executable(final_massive_test     ts_store_final_massive_test/final_massive_test.cpp   ts_store.hpp)


