cmake_minimum_required(VERSION 3.20)
project(secretariatpro_replay_core_tests LANGUAGES CXX)

enable_testing()

add_executable(timeline-tests timeline-tests.cpp ../src/replay-timeline.cpp)
target_include_directories(timeline-tests PRIVATE ../src)
target_compile_features(timeline-tests PRIVATE cxx_std_17)

if(MSVC)
  target_compile_options(timeline-tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(timeline-tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME timeline-tests COMMAND timeline-tests)

add_executable(layout-tests layout-tests.cpp ../src/replay-layout.cpp)
target_include_directories(layout-tests PRIVATE ../src)
target_compile_features(layout-tests PRIVATE cxx_std_17)

if(MSVC)
  target_compile_options(layout-tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(layout-tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME layout-tests COMMAND layout-tests)
