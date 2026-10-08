cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)
option(BUILD_TOOLS "Build the link-audio-probe test peer" ON)
option(BUILD_TESTS "Build the unit tests" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
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

include(deps/link/AbletonLinkConfig.cmake)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Ableton::Link)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/input/buffer-clock.cpp
    src/input/link-audio-input.cpp
    src/input/link-audio-input-source.cpp
    src/session/link-session.cpp
)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)
target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

if(BUILD_TOOLS)
  add_executable(link-audio-probe tools/link-audio-probe/main.cpp)
  target_link_libraries(link-audio-probe PRIVATE Ableton::Link)
  target_compile_features(link-audio-probe PRIVATE cxx_std_17)
  # GCC reports uninitialized members inside Link's own sink headers.
  target_compile_options(
    link-audio-probe
    PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-error=uninitialized -Wno-error=maybe-uninitialized>
  )
endif()

if(BUILD_TESTS)
  enable_testing()
  add_executable(buffer-clock-test tests/input/buffer-clock-test.cpp src/input/buffer-clock.cpp)
  target_include_directories(buffer-clock-test PRIVATE src)
  target_compile_features(buffer-clock-test PRIVATE cxx_std_17)
  add_test(NAME buffer-clock COMMAND buffer-clock-test)
endif()
