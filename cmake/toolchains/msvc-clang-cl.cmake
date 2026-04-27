# cmake/toolchains/msvc-clang-cl.cmake
#
# Use Clang-cl as the MSVC-compatible compiler driver. Useful for CFG/CET
# experiments and to validate that the codebase is multi-compiler clean.
#
# Usage (PowerShell, with LLVM + VS Build Tools installed):
#  cmake -B build -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/msvc-clang-cl.cmake
#  cmake --build build

set(CMAKE_SYSTEM_NAME  Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER  clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_LINKER  lld-link)
set(CMAKE_AR  llvm-lib  CACHE FILEPATH "")

# Match MSVC ABI explicitly.
add_compile_options(/clang:-march=x86-64-v2)

add_compile_definitions(
  _WIN32_WINNT=0x0A00
  NTDDI_VERSION=0x0A000007
  WINVER=0x0A00
  WIN32_LEAN_AND_MEAN
  _CRT_SECURE_NO_WARNINGS
)
