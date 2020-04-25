cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

project(caf_tracing_data VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CAF_ENABLE_ACTOR_PROFILER
    ON
    CACHE BOOL "Enable experimental profiler API" FORCE)
set(CAF_ENABLE_EXAMPLES
    OFF
    CACHE BOOL "Build small programs showcasing CAF features" FORCE)
set(CAF_ENABLE_TESTING
    OFF
    CACHE BOOL "Build unit test suites" FORCE)
add_subdirectory(external/actor-framework)

set(APP_NAME caf_tracing_data_app)
add_executable(${APP_NAME} src/main.cpp)

target_link_libraries(
  ${APP_NAME}
  PRIVATE
  Threads::Threads
  ${CMAKE_DL_LIBS}
  CAF::core
  CAF::io
)

target_include_directories(
  ${APP_NAME}
  PRIVATE
  ${CAF_INCLUDE_DIRS}
)

target_compile_definitions(
  ${APP_NAME}
  PRIVATE
  CAF_ENABLE_ACTOR_PROFILER
)
