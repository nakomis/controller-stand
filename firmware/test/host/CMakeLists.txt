# Host build of the charge estimator and its tests. No ESP-IDF needed.
cmake_minimum_required(VERSION 3.16)
project(charge_estimator_host_tests C)

set(CMAKE_C_STANDARD 11)
set(EST_DIR ${CMAKE_CURRENT_LIST_DIR}/../../components/charge_estimator)

option(COVERAGE "Build with gcov instrumentation" OFF)

add_executable(test_charge_estimator
    test_charge_estimator.c
    ${EST_DIR}/charge_estimator.c)
target_include_directories(test_charge_estimator PRIVATE ${EST_DIR}/include)
target_compile_options(test_charge_estimator PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_charge_estimator PRIVATE m)
if(COVERAGE)
    target_compile_options(test_charge_estimator PRIVATE --coverage -O0)
    target_link_options(test_charge_estimator PRIVATE --coverage)
endif()

enable_testing()
add_test(NAME charge_estimator COMMAND test_charge_estimator)
