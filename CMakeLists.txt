# =============================================================================
#  Wraith - top-level build
# =============================================================================
#
#  Modern, reliable, stealth PE loader for Windows x64.
#
#  Quick start:
#  cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-x86_64.cmake
#  cmake --build build -j
#
#  See cmake/options.cmake for all build switches.
# =============================================================================

cmake_minimum_required(VERSION 3.20)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cmake)

project(Wraith
  VERSION  ${WRAITH_VERSION_MAJOR}.${WRAITH_VERSION_MINOR}.${WRAITH_VERSION_PATCH}
  DESCRIPTION "Modern, reliable, stealth PE loader for Windows x64"
  LANGUAGES  C CXX ASM
)

# x64-only target by design.
include(DetectArch)

# All build switches in one place.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/options.cmake)
include(HardenFlags)
include(WineTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

wr_print_config()

# -----------------------------------------------------------------------------
# Library sources.
# -----------------------------------------------------------------------------
set(WRAITH_SOURCES
  src/core/wr_status.c
  src/core/wr_context.c
  src/pe/pe_validate.c
  src/pe/pe_iter.c
  src/pe/pe_image_metrics.c
  src/mapping/map_dispatch.c
  src/mapping/map_private_rwx.c
  src/runtime/rt_api.c
  src/runtime/rt_api_baseline.c
  src/runtime/rt_pebwalk.c
  src/runtime/rt_resolver.c
  src/stealth/hashing/hash_djb2.c
  src/loader/loader_pipeline.c
  src/loader/loader_sections.c
  src/loader/loader_relocs.c
  src/loader/loader_imports.c
  src/loader/loader_imports_bound.c
  src/loader/loader_imports_delay.c
  src/loader/loader_finalize.c
  src/loader/loader_seh_x64.c
  src/loader/loader_tls.c
  src/loader/loader_entry.c
  src/loader/loader_api.c
  src/exports/export_lookup.c
  src/exports/export_forward.c
  src/resource/resource_find.c
  src/resource/resource_load.c
  src/resource/resource_string.c
)

if(WRAITH_USE_API_HASHING)
  list(APPEND WRAITH_SOURCES
  src/runtime/rt_api_ntapi.c
  )
endif()

if(WRAITH_USE_INDIRECT_SYSCALLS)
  list(APPEND WRAITH_SOURCES
  src/syscalls/sc_engine.c
  src/syscalls/sc_ssn_resolver.c
  src/syscalls/sc_gadget_finder.c
  src/syscalls/sc_trampoline_x64.S
  )
endif()

if(WRAITH_USE_PHANTOM_HOLLOWING)
  list(APPEND WRAITH_SOURCES
  src/mapping/map_phantom.c
  src/mapping/map_phantom_host_picker.c
  )
endif()

if(WRAITH_USE_MODULE_STOMPING)
  list(APPEND WRAITH_SOURCES
  src/mapping/map_stomping.c
  )
  if(NOT WRAITH_USE_PHANTOM_HOLLOWING)
  # The host picker is shared - pull it in even when phantom is off.
  list(APPEND WRAITH_SOURCES
  src/mapping/map_phantom_host_picker.c
  )
  endif()
endif()

# Mockingjay is always compiled - the scanner returns
# WRAITH_E_MAP_NO_HOST_DLL when the host process has no qualifying region.
list(APPEND WRAITH_SOURCES
  src/mapping/map_mockingjay.c
  src/mapping/map_mockingjay_scanner.c
)

if(WRAITH_USE_PEB_LINKAGE)
  list(APPEND WRAITH_SOURCES
  src/stealth/peb_link/peb_link.c
  src/stealth/peb_link/peb_link_masquerade.c
  )
endif()

if(WRAITH_USE_SLEEP_OBFUSCATION)
  list(APPEND WRAITH_SOURCES
  src/stealth/sleep/sleep.c
  src/stealth/sleep/sleep_xor_baseline.c
  src/stealth/sleep/sleep_cronos.c
  )
endif()

if(WRAITH_USE_UNHOOK_NTDLL)
  list(APPEND WRAITH_SOURCES
  src/stealth/unhook/unhook.c
  )
endif()

if(WRAITH_USE_ETW_PATCH)
  list(APPEND WRAITH_SOURCES
  src/stealth/etw/etw_patch.c
  )
endif()

if(WRAITH_USE_AMSI_PATCH)
  list(APPEND WRAITH_SOURCES
  src/stealth/amsi/amsi_patch.c
  )
endif()

if(WRAITH_USE_HWBP_HOOKS)
  list(APPEND WRAITH_SOURCES
  src/stealth/hwbp/hwbp.c
  )
endif()

if(WRAITH_USE_PRIVATE_NTDLL)
  list(APPEND WRAITH_SOURCES
  src/stealth/private_ntdll/private_ntdll.c
  )
endif()

if(WRAITH_USE_THREADLESS_EXEC)
  list(APPEND WRAITH_SOURCES
  src/stealth/threadless/threadless.c
  )
endif()

if(WRAITH_USE_PAGE_GUARD_ENCRYPT)
  list(APPEND WRAITH_SOURCES
  src/stealth/page_guard/page_guard.c
  )
endif()

if(WRAITH_USE_HEAP_MASQUERADE)
  list(APPEND WRAITH_SOURCES
  src/stealth/heap_masq/heap_masq.c
  )
endif()
if(WRAITH_USE_ANTI_DEBUG_SPOOF)
  list(APPEND WRAITH_SOURCES
  src/stealth/anti_debug/anti_debug.c
  )
endif()
if(WRAITH_USE_HOST_IAT_REDIRECT)
  list(APPEND WRAITH_SOURCES
  src/stealth/host_iat/host_iat.c
  )
endif()

# Loader-as-shellcode (PIC) build flag - reserved for future minor
# release; current toggle simply emits a status line.
if(WRAITH_BUILD_PIC)
  message(STATUS "WRAITH_BUILD_PIC=ON: PIC mode flagged "
  "(shellcode-blob output not yet implemented).")
endif()

# -----------------------------------------------------------------------------
# Codegen: tools/hashgen.py emits ${CMAKE_BINARY_DIR}/generated/wr_api_hashes.h
# from tools/api_list.txt. Re-runs whenever either input changes.
# -----------------------------------------------------------------------------
find_package(Python3 COMPONENTS Interpreter)
set(WRAITH_HASHES_HDR "${CMAKE_BINARY_DIR}/generated/wr_api_hashes.h")
if(Python3_FOUND)
  add_custom_command(
  OUTPUT  "${WRAITH_HASHES_HDR}"
  COMMAND ${Python3_EXECUTABLE}
  "${CMAKE_SOURCE_DIR}/tools/hashgen.py"
  --input  "${CMAKE_SOURCE_DIR}/tools/api_list.txt"
  --output "${WRAITH_HASHES_HDR}"
  DEPENDS "${CMAKE_SOURCE_DIR}/tools/hashgen.py"
  "${CMAKE_SOURCE_DIR}/tools/api_list.txt"
  COMMENT "hashgen: generating wr_api_hashes.h"
  VERBATIM
  )
  add_custom_target(wr_hashes_generate
  DEPENDS "${WRAITH_HASHES_HDR}"
  )
else()
  message(STATUS "Python3 not found - wr_api_hashes.h will use fallback constants")
endif()

set(WRAITH_PUBLIC_HEADERS
  include/wraith/wraith.h
  include/wraith/wraith_introspect.h
  include/wraith/wraith_loader.h
  include/wraith/wraith_options.h
  include/wraith/wraith_resource.h
  include/wraith/wraith_status.h
  include/wraith/wraith_stealth.h
  include/wraith/wraith_types.h
)

if(WRAITH_BUILD_STATIC)
  add_library(wraith STATIC ${WRAITH_SOURCES})
  set_target_properties(wraith PROPERTIES
  OUTPUT_NAME wraith
  POSITION_INDEPENDENT_CODE ON
  )
  target_include_directories(wraith
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_BINARY_DIR}/generated
  )
  if(Python3_FOUND)
  add_dependencies(wraith wr_hashes_generate)
  endif()
  wr_apply_hardening(wraith)
  wr_apply_feature_defines(wraith)
endif()

if(WRAITH_BUILD_SHARED)
  add_library(wraith_shared SHARED ${WRAITH_SOURCES})
  set_target_properties(wraith_shared PROPERTIES
  OUTPUT_NAME wraith
  PREFIX ""
  )
  target_include_directories(wraith_shared
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_BINARY_DIR}/generated
  )
  if(Python3_FOUND)
  add_dependencies(wraith_shared wr_hashes_generate)
  endif()
  wr_apply_hardening(wraith_shared)
  wr_apply_feature_defines(wraith_shared)
endif()

# -----------------------------------------------------------------------------
# Tests / examples
# -----------------------------------------------------------------------------
if(WRAITH_BUILD_TESTS)
  enable_testing()
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/CMakeLists.txt")
  add_subdirectory(tests/integration)
  endif()
endif()

if(WRAITH_BUILD_EXAMPLES
  AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/full_stealth_chain/CMakeLists.txt")
  add_subdirectory(examples/full_stealth_chain)
endif()
