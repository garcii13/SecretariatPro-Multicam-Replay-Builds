cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)
if(TARGET plugin-support)
  target_link_libraries(plugin-support PRIVATE OBS::libobs)
endif()

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/iso-capture.cpp
    src/iso-capture.hpp
    src/replay-dock.cpp
    src/replay-dock.hpp
    src/replay-engine.cpp
    src/replay-engine.hpp
    src/replay-hotkeys.cpp
    src/replay-hotkeys.hpp
    src/replay-layout.cpp
    src/replay-layout.hpp
    src/replay-media-provider.hpp
    src/replay-output-source.cpp
    src/replay-output-source.hpp
    src/replay-settings.cpp
    src/replay-settings.hpp
    src/replay-timeline.cpp
    src/replay-timeline.hpp
)

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(MSVC)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W4 /permissive-)
else()
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

include(CTest)
if(BUILD_TESTING)
  add_executable(secretariatpro-encoder-policy-tests tests/encoder-policy-tests.cpp)
  target_include_directories(secretariatpro-encoder-policy-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_compile_features(secretariatpro-encoder-policy-tests PRIVATE cxx_std_17)
  add_test(NAME encoder-policy COMMAND secretariatpro-encoder-policy-tests)

  add_executable(secretariatpro-replay-timeline-tests tests/timeline-tests.cpp src/replay-timeline.cpp)
  target_include_directories(secretariatpro-replay-timeline-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_compile_features(secretariatpro-replay-timeline-tests PRIVATE cxx_std_17)
  add_test(NAME replay-timeline COMMAND secretariatpro-replay-timeline-tests)

  add_executable(secretariatpro-replay-layout-tests tests/layout-tests.cpp src/replay-layout.cpp)
  target_include_directories(secretariatpro-replay-layout-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_compile_features(secretariatpro-replay-layout-tests PRIVATE cxx_std_17)
  add_test(NAME replay-layout COMMAND secretariatpro-replay-layout-tests)
  foreach(test_target secretariatpro-encoder-policy-tests secretariatpro-replay-timeline-tests secretariatpro-replay-layout-tests)
    if(MSVC)
      target_compile_options(${test_target} PRIVATE /UNDEBUG)
    else()
      target_compile_options(${test_target} PRIVATE -UNDEBUG)
    endif()
  endforeach()
endif()
