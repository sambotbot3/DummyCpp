cmake_minimum_required(VERSION 3.20)

project(DummyCpp VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(dpp_utils src/string_utils.cpp)
target_include_directories(dpp_utils PUBLIC include)

add_subdirectory(src/parser)
add_subdirectory(src/convert)
add_subdirectory(inject)

add_library(dpp_transpiler src/transpiler.cpp)
target_include_directories(dpp_transpiler PUBLIC include)
target_link_libraries(dpp_transpiler PUBLIC dpp_parser dpp_convert)

add_executable(dpp src/main.cpp)
target_link_libraries(dpp PRIVATE dpp_transpiler)

target_compile_options(dpp_parser PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

target_compile_options(dpp_convert PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

target_compile_options(dpp_utils PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

target_compile_options(dpp_transpiler PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

target_compile_options(dpp PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

if(BUILD_TESTING)
  add_executable(dpp_parser_tests tests/parser_tests.cpp)
  target_link_libraries(dpp_parser_tests PRIVATE dpp_parser)
  target_compile_options(dpp_parser_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
  )
  add_test(NAME dpp_parser_tests COMMAND dpp_parser_tests)
endif()
