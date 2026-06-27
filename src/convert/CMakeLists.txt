add_library(dpp_convert
  algorithm.cpp
  assertions.cpp
  basic.cpp
  cleanup.cpp
  free_functions.cpp
  iostream.cpp
  map.cpp
  memory.cpp
  records.cpp
  string.cpp
  templates.cpp
  vector.cpp
)

target_include_directories(dpp_convert PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(dpp_convert PRIVATE dpp_utils dpp_parser)
