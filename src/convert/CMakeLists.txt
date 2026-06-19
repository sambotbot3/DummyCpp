add_library(dpp_convert
  basic.cpp
  iostream.cpp
  map.cpp
  memory.cpp
  records.cpp
  vector.cpp
)

target_include_directories(dpp_convert PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)
