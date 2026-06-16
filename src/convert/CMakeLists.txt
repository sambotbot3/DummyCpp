add_library(dpp_convert
  basic.cpp
  iostream.cpp
)

target_include_directories(dpp_convert PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)

