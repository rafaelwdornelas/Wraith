# cmake/toolchains/mingw-x86_64.cmake
#
# Cross-compile Wraith from Linux to Windows x64 using MinGW-w64.
#
# Usage:
#  cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-x86_64.cmake
#  cmake --build build -j

set(CMAKE_SYSTEM_NAME  Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(WRAITH_CROSS_PREFIX x86_64-w64-mingw32 CACHE STRING "MinGW cross-compile prefix")

set(CMAKE_C_COMPILER  ${WRAITH_CROSS_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${WRAITH_CROSS_PREFIX}-g++)
set(CMAKE_RC_COMPILER  ${WRAITH_CROSS_PREFIX}-windres)
set(CMAKE_AR  ${WRAITH_CROSS_PREFIX}-ar  CACHE FILEPATH "")
set(CMAKE_RANLIB  ${WRAITH_CROSS_PREFIX}-ranlib  CACHE FILEPATH "")
set(CMAKE_STRIP  ${WRAITH_CROSS_PREFIX}-strip  CACHE FILEPATH "")

set(CMAKE_FIND_ROOT_PATH /usr/${WRAITH_CROSS_PREFIX})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Static link libgcc/libstdc++ so the produced binaries don't depend on
# MinGW runtime DLLs. Helpful for portable test artifacts under wine64.
set(CMAKE_C_FLAGS_INIT  "-static-libgcc")
set(CMAKE_CXX_FLAGS_INIT "-static-libgcc -static-libstdc++")

# Tighten Windows headers - we want the modern PE definitions.
add_compile_definitions(
  _WIN32_WINNT=0x0A00  # Windows 10
  NTDDI_VERSION=0x0A000007 # NTDDI_WIN10_VB (Win10 1809)
  WINVER=0x0A00
  WIN32_LEAN_AND_MEAN
)
