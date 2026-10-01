cmake_minimum_required(VERSION 3.21)
project(ParadigmOriginScoreEditor VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Multimedia)

add_executable(ParadigmOriginScoreEditor WIN32
    src/main.cpp
    src/chart.cpp
    src/editor.cpp
)
target_include_directories(ParadigmOriginScoreEditor PRIVATE src)
target_link_libraries(ParadigmOriginScoreEditor PRIVATE Qt6::Widgets Qt6::Multimedia)

include(CTest)
if(BUILD_TESTING)
    add_executable(chart_tests tests/chart_tests.cpp src/chart.cpp)
    target_include_directories(chart_tests PRIVATE src)
    target_link_libraries(chart_tests PRIVATE Qt6::Core)
    add_test(NAME chart_tests COMMAND chart_tests)
endif()
