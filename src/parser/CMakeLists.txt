add_library(dpp_parser
  lexer.cpp
  parser.cpp
  preprocessor.cpp
  syntax_checker.cpp
)

target_include_directories(dpp_parser PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)

target_include_directories(dpp_parser PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)
