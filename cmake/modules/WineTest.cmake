# cmake/modules/WineTest.cmake
#
# Helpers to register Windows test executables to run under wine64 on Linux.

function(wr_add_wine_test name target)
  if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND NOT CMAKE_CROSSCOMPILING)
  # Native Windows build - use the binary directly.
  add_test(NAME ${name} COMMAND ${target})
  else()
  find_program(WRAITH_WINE64 NAMES wine64 wine)
  if(NOT WRAITH_WINE64)
  message(WARNING "wine64 not found - skipping test ${name}")
  return()
  endif()
  add_test(NAME ${name} COMMAND ${WRAITH_WINE64} $<TARGET_FILE:${target}>)
  set_tests_properties(${name} PROPERTIES
  ENVIRONMENT "WINEDEBUG=-all;WINEPREFIX=$ENV{HOME}/.wine"
  )
  endif()
endfunction()
