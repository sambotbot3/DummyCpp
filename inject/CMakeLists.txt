add_library(dpp_inject STATIC
  c/dpp_vector.c
)

target_include_directories(dpp_inject PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/c
)

target_compile_options(dpp_inject PRIVATE
  $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)
