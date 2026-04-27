# tests/integration/fixtures/forwarder_dll/CMakeLists.txt
#
# Builds forwarder_dll.dll from a stub .c + a .def file with a single
# forwarded export. MinGW's ld accepts the .def directly when listed as
# a source for a SHARED library.

add_library(forwarder_dll SHARED
  forwarder_stub.c
  forwarder_dll.def
)

set_target_properties(forwarder_dll PROPERTIES
  PREFIX ""
  SUFFIX ".dll"
)

# Don't apply Wraith's -Werror=missing-prototypes etc. to the fixture.
if(NOT MSVC)
  target_compile_options(forwarder_dll PRIVATE -w)
endif()
