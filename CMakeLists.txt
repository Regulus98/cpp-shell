cmake_minimum_required(VERSION 3.20)
project(cppshell LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(READLINE REQUIRED readline)

add_executable(cppshell
  src/main.cpp
  src/shell.cpp
  src/tokenizer.cpp
  src/parser.cpp
  src/exec.cpp
  src/jobs.cpp
  src/sys.cpp
)

target_compile_options(cppshell PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

target_include_directories(cppshell PRIVATE ${READLINE_INCLUDE_DIRS})
target_link_libraries(cppshell PRIVATE ${READLINE_LIBRARIES})
