# tests/integration/fixtures/payload_dll/CMakeLists.txt
#
# Tiny fixture DLL with a single integer-arithmetic export. Used by
# the integration test suite as a deterministic load target.

add_library(payload_dll SHARED payload.c)

set_target_properties(payload_dll PROPERTIES
  PREFIX ""
  OUTPUT_NAME "payload"
  SUFFIX ".dll"
)

if(NOT MSVC)
  target_compile_options(payload_dll PRIVATE -w)
endif()
