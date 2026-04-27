# tests/integration/fixtures/seh_dll/CMakeLists.txt
add_library(seh_dll SHARED seh_dll.c)
set_target_properties(seh_dll PROPERTIES
  PREFIX ""
  SUFFIX ".dll"
)
if(NOT MSVC)
  target_compile_options(seh_dll PRIVATE -w)
endif()
