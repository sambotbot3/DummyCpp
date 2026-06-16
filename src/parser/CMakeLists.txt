add_library(dpp_parser
  parser.cpp
  syntax_checker.cpp
)

target_include_directories(dpp_parser PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)

