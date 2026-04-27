# cmake/modules/DetectArch.cmake
#
# Wraith is x64-only. This module hard-fails on non-x64 builds and
# exposes WRAITH_ARCH_X64 as a sanity-check definition.

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR
  "Wraith is x64-only (CMAKE_SIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P}). "
  "x86 / ARM are out of scope for v2.0. See README 'Why a v2?' section.")
endif()

set(WRAITH_ARCH "x86_64" CACHE INTERNAL "Detected target architecture")

add_compile_definitions(WRAITH_ARCH_X64=1)
