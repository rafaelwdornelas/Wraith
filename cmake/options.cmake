# cmake/options.cmake
#
# All Wraith build options live here. CMakeLists.txt includes this
# file early so every subdirectory sees a coherent set of flags.
#
# Convention:
#  WRAITH_BUILD_*  - artifacts to produce (static, shared, tests, examples, fuzz)
#  WRAITH_USE_*  - opt-in stealth modules (OFF by default)
#  WRAITH_HARDEN_*  - compiler-driven hardening
#  WRAITH_PROFILE  - preset bundle that flips multiple flags coherently
#  WRAITH_*_ALGO  - algorithm selection within a feature

# ----------------------------------------------------------------------------
# Build artifacts
# ----------------------------------------------------------------------------
option(WRAITH_BUILD_STATIC  "Build static library (libwraith.a)"  ON)
option(WRAITH_BUILD_SHARED  "Build shared library (wraith.dll)"  OFF)
option(WRAITH_BUILD_TESTS  "Build integration tests"  ON)
option(WRAITH_BUILD_EXAMPLES  "Build example consumers"  ON)
option(WRAITH_BUILD_FUZZ  "Build libFuzzer harnesses (requires Clang)"  OFF)
option(WRAITH_BUILD_PIC  "Build position-independent shellcode blob"  OFF)

option(WRAITH_UNICODE  "Define UNICODE/_UNICODE for resource APIs"  OFF)

# ----------------------------------------------------------------------------
# Reliability features (ON by default)
# ----------------------------------------------------------------------------
option(WRAITH_FORWARDED_EXPORTS  "Resolve forwarded exports (kernel32.Sleep -> ...)" ON)
option(WRAITH_DELAY_LOAD_IMPORTS  "Resolve delay-load imports (ImgDelayDescr)"  ON)
option(WRAITH_BOUND_IMPORTS  "Honor bound imports (timestamp match -> skip)"  ON)
option(WRAITH_REGISTER_SEH_X64  "Call RtlAddFunctionTable for x64 .pdata (SEH)"  ON)
option(WRAITH_TLS_FULL_LIFECYCLE  "TLS callbacks for all DLL_PROCESS/THREAD reasons"  ON)
option(WRAITH_RW_TO_RX_HYGIENE  "Forbid PAGE_EXECUTE_READWRITE intermediate"  ON)

# ----------------------------------------------------------------------------
# Classic stealth (OFF by default - opt-in)
# ----------------------------------------------------------------------------
option(WRAITH_USE_API_HASHING  "DJB2/FNV1a-hashed API resolution"  OFF)
option(WRAITH_USE_PEB_WALK  "Resolve modules via PEB.Ldr (no GetModuleHandle)" OFF)
option(WRAITH_USE_INDIRECT_SYSCALLS "Hell's Hall indirect syscalls"  OFF)
option(WRAITH_USE_PEB_LINKAGE  "Insert into PEB.Ldr lists with masquerade name"  OFF)
option(WRAITH_USE_PHANTOM_HOLLOWING "NtCreateSection(SEC_IMAGE) backing"  OFF)
option(WRAITH_USE_MODULE_STOMPING  "Module stomping strategy (lab-only)"  OFF)
option(WRAITH_USE_SLEEP_OBFUSCATION "Encrypt .text during idle (XOR/Ekko/Foliage/Cronos)" OFF)
option(WRAITH_USE_UNHOOK_NTDLL  "Refresh ntdll.text from disk (legacy)"  OFF)
option(WRAITH_USE_ETW_PATCH  "Patch EtwEventWrite to xor eax,eax; ret"  OFF)
option(WRAITH_USE_AMSI_PATCH  "Patch AmsiScanBuffer to AMSI_RESULT_CLEAN"  OFF)

# ----------------------------------------------------------------------------
# Bleeding-edge tier
# ----------------------------------------------------------------------------
option(WRAITH_USE_STACK_SPOOF  "SilentMoonwalk-style synthetic frame spoof"  OFF)
option(WRAITH_USE_HWBP_HOOKS  "DR0-DR3 hardware breakpoint hooks via VEH"  OFF)
option(WRAITH_USE_PRIVATE_NTDLL  "Map clean second copy of ntdll from disk"  OFF)
option(WRAITH_USE_THREADLESS_EXEC  "Hijack TpAllocWork / RtlRegisterWait"  OFF)
option(WRAITH_USE_PAGE_GUARD_ENCRYPT "Lazy per-page guard-driven self-encryption"  OFF)
option(WRAITH_USE_HEAP_MASQUERADE  "RtlCreateHeap rooted in legit MEM_IMAGE"  OFF)
option(WRAITH_USE_ANTI_DEBUG_SPOOF  "Passive PEB / debug flag spoofing"  OFF)
option(WRAITH_USE_HOST_IAT_REDIRECT "Patch host IAT to route Sleep through obf"  OFF)

# ----------------------------------------------------------------------------
# Algorithm selection (only meaningful when corresponding feature is ON)
# ----------------------------------------------------------------------------
set(WRAITH_HASH_ALGO  "djb2"  CACHE STRING "API hashing algorithm: djb2 | fnv1a")
set_property(CACHE WRAITH_HASH_ALGO PROPERTY STRINGS djb2 fnv1a)

set(WRAITH_SLEEP_ALGO  "ekko"  CACHE STRING "Sleep obfuscation algorithm: xor | ekko | foliage | cronos")
set_property(CACHE WRAITH_SLEEP_ALGO PROPERTY STRINGS xor ekko foliage cronos)

set(WRAITH_SC_RESOLVER  "hellshall" CACHE STRING "SSN resolver: hellshall | freshycalls")
set_property(CACHE WRAITH_SC_RESOLVER PROPERTY STRINGS hellshall freshycalls)

set(WRAITH_MAP_DEFAULT  "private_rwx" CACHE STRING "Default mapping strategy: private_rwx | phantom | stomping | mockingjay")
set_property(CACHE WRAITH_MAP_DEFAULT PROPERTY STRINGS private_rwx phantom stomping mockingjay)

# ----------------------------------------------------------------------------
# Compiler hardening
# ----------------------------------------------------------------------------
option(WRAITH_HARDEN_CFG  "Compile with /guard:cf or -fcf-protection"  OFF)
option(WRAITH_HARDEN_CET  "CET shadow stack support (Win11 + Tiger Lake+)"  OFF)
option(WRAITH_HARDEN_STACK_PROTECT  "/GS or -fstack-protector-strong"  ON)
option(WRAITH_HARDEN_NO_CRT  "Build without CRT (-nostdlib + custom intrinsics)" OFF)

# ----------------------------------------------------------------------------
# Diagnostics (OFF in release builds)
# ----------------------------------------------------------------------------
option(WRAITH_DEBUG_LOG  "Verbose OutputDebugString logging"  OFF)
option(WRAITH_TRACE_PIPELINE  "Trace load pipeline phases"  OFF)

# ----------------------------------------------------------------------------
# Profile presets
# ----------------------------------------------------------------------------
# A profile flips a coherent bundle of flags. Resolved BEFORE any other logic
# in CMakeLists.txt so it acts as defaults the user can still override.
#
#  default  - reliability ON, stealth OFF (lightweight)
#  teaching  - everything compiled, runtime-flag controls activation
#  paranoid-classic - classic full chain (hashing + syscalls + phantom + sleep + spoof)
#  paranoid-full  - bleeding-edge (everything ON, hardened, no CRT)
#  minimal  - smallest binary, no stealth, no reliability extras
set(WRAITH_PROFILE "default" CACHE STRING "Preset bundle: default | teaching | paranoid-classic | paranoid-full | minimal")
set_property(CACHE WRAITH_PROFILE PROPERTY STRINGS default teaching paranoid-classic paranoid-full minimal)

function(_wraith_profile_apply)
  if(WRAITH_PROFILE STREQUAL "default")
  # No overrides - uses the option defaults defined above.

  elseif(WRAITH_PROFILE STREQUAL "teaching")
  set(WRAITH_USE_API_HASHING  ON  PARENT_SCOPE)
  set(WRAITH_USE_PEB_WALK  ON  PARENT_SCOPE)
  set(WRAITH_USE_INDIRECT_SYSCALLS  ON  PARENT_SCOPE)
  set(WRAITH_USE_PEB_LINKAGE  ON  PARENT_SCOPE)
  set(WRAITH_USE_PHANTOM_HOLLOWING  ON  PARENT_SCOPE)
  set(WRAITH_USE_MODULE_STOMPING  ON  PARENT_SCOPE)
  set(WRAITH_USE_SLEEP_OBFUSCATION  ON  PARENT_SCOPE)
  set(WRAITH_USE_UNHOOK_NTDLL  ON  PARENT_SCOPE)
  set(WRAITH_USE_ETW_PATCH  ON  PARENT_SCOPE)
  set(WRAITH_USE_AMSI_PATCH  ON  PARENT_SCOPE)
  set(WRAITH_USE_STACK_SPOOF  ON  PARENT_SCOPE)
  set(WRAITH_USE_HWBP_HOOKS  ON  PARENT_SCOPE)
  set(WRAITH_USE_PRIVATE_NTDLL  ON  PARENT_SCOPE)
  set(WRAITH_USE_THREADLESS_EXEC  ON  PARENT_SCOPE)
  set(WRAITH_USE_PAGE_GUARD_ENCRYPT ON  PARENT_SCOPE)
  set(WRAITH_USE_HEAP_MASQUERADE  ON  PARENT_SCOPE)
  set(WRAITH_USE_ANTI_DEBUG_SPOOF  ON  PARENT_SCOPE)
  set(WRAITH_USE_HOST_IAT_REDIRECT  ON  PARENT_SCOPE)
  set(WRAITH_DEBUG_LOG  ON  PARENT_SCOPE)
  set(WRAITH_TRACE_PIPELINE  ON  PARENT_SCOPE)

  elseif(WRAITH_PROFILE STREQUAL "paranoid-classic")
  set(WRAITH_USE_API_HASHING  ON  PARENT_SCOPE)
  set(WRAITH_USE_PEB_WALK  ON  PARENT_SCOPE)
  set(WRAITH_USE_INDIRECT_SYSCALLS  ON  PARENT_SCOPE)
  set(WRAITH_USE_PEB_LINKAGE  ON  PARENT_SCOPE)
  set(WRAITH_USE_PHANTOM_HOLLOWING  ON  PARENT_SCOPE)
  set(WRAITH_USE_SLEEP_OBFUSCATION  ON  PARENT_SCOPE)
  set(WRAITH_SLEEP_ALGO  "ekko" PARENT_SCOPE)
  set(WRAITH_USE_ETW_PATCH  ON  PARENT_SCOPE)
  set(WRAITH_USE_STACK_SPOOF  ON  PARENT_SCOPE)
  set(WRAITH_USE_PRIVATE_NTDLL  ON  PARENT_SCOPE)
  set(WRAITH_SC_RESOLVER  "freshycalls" PARENT_SCOPE)

  elseif(WRAITH_PROFILE STREQUAL "paranoid-full")
  set(WRAITH_USE_API_HASHING  ON  PARENT_SCOPE)
  set(WRAITH_USE_PEB_WALK  ON  PARENT_SCOPE)
  set(WRAITH_USE_INDIRECT_SYSCALLS  ON  PARENT_SCOPE)
  set(WRAITH_USE_PEB_LINKAGE  ON  PARENT_SCOPE)
  set(WRAITH_USE_PHANTOM_HOLLOWING  ON  PARENT_SCOPE)
  set(WRAITH_USE_SLEEP_OBFUSCATION  ON  PARENT_SCOPE)
  set(WRAITH_SLEEP_ALGO  "cronos" PARENT_SCOPE)
  set(WRAITH_USE_ETW_PATCH  ON  PARENT_SCOPE)
  set(WRAITH_USE_AMSI_PATCH  ON  PARENT_SCOPE)
  set(WRAITH_USE_STACK_SPOOF  ON  PARENT_SCOPE)
  set(WRAITH_USE_HWBP_HOOKS  ON  PARENT_SCOPE)
  set(WRAITH_USE_PRIVATE_NTDLL  ON  PARENT_SCOPE)
  set(WRAITH_USE_THREADLESS_EXEC  ON  PARENT_SCOPE)
  set(WRAITH_USE_PAGE_GUARD_ENCRYPT ON  PARENT_SCOPE)
  set(WRAITH_USE_HEAP_MASQUERADE  ON  PARENT_SCOPE)
  set(WRAITH_USE_ANTI_DEBUG_SPOOF  ON  PARENT_SCOPE)
  set(WRAITH_SC_RESOLVER  "freshycalls" PARENT_SCOPE)
  set(WRAITH_MAP_DEFAULT  "phantom" PARENT_SCOPE)
  set(WRAITH_HARDEN_NO_CRT  ON  PARENT_SCOPE)

  elseif(WRAITH_PROFILE STREQUAL "minimal")
  set(WRAITH_BUILD_TESTS  OFF PARENT_SCOPE)
  set(WRAITH_BUILD_EXAMPLES  OFF PARENT_SCOPE)
  set(WRAITH_FORWARDED_EXPORTS  OFF PARENT_SCOPE)
  set(WRAITH_DELAY_LOAD_IMPORTS  OFF PARENT_SCOPE)
  set(WRAITH_BOUND_IMPORTS  OFF PARENT_SCOPE)

  else()
  message(FATAL_ERROR "Unknown WRAITH_PROFILE: ${WRAITH_PROFILE}. Allowed: default | teaching | paranoid-classic | paranoid-full | minimal")
  endif()
endfunction()

_wraith_profile_apply()

# ----------------------------------------------------------------------------
# Echo configuration (helpful for CI logs)
# ----------------------------------------------------------------------------
function(wr_print_config)
  message(STATUS "")
  message(STATUS "================ Wraith build configuration ================")
  message(STATUS "  Profile  : ${WRAITH_PROFILE}")
  message(STATUS "  Hash algo  : ${WRAITH_HASH_ALGO}")
  message(STATUS "  Sleep algo  : ${WRAITH_SLEEP_ALGO}")
  message(STATUS "  Syscall resolver: ${WRAITH_SC_RESOLVER}")
  message(STATUS "  Map default  : ${WRAITH_MAP_DEFAULT}")
  message(STATUS "  ----")
  message(STATUS "  Reliability  : forwarders=${WRAITH_FORWARDED_EXPORTS} delay=${WRAITH_DELAY_LOAD_IMPORTS} bound=${WRAITH_BOUND_IMPORTS}")
  message(STATUS "  seh_x64=${WRAITH_REGISTER_SEH_X64} tls_full=${WRAITH_TLS_FULL_LIFECYCLE} rw_to_rx=${WRAITH_RW_TO_RX_HYGIENE}")
  message(STATUS "  Classic stealth : hashing=${WRAITH_USE_API_HASHING} pebwalk=${WRAITH_USE_PEB_WALK} syscalls=${WRAITH_USE_INDIRECT_SYSCALLS}")
  message(STATUS "  peb_link=${WRAITH_USE_PEB_LINKAGE} phantom=${WRAITH_USE_PHANTOM_HOLLOWING} stomp=${WRAITH_USE_MODULE_STOMPING}")
  message(STATUS "  sleep=${WRAITH_USE_SLEEP_OBFUSCATION} unhook=${WRAITH_USE_UNHOOK_NTDLL} etw=${WRAITH_USE_ETW_PATCH} amsi=${WRAITH_USE_AMSI_PATCH}")
  message(STATUS "  Bleeding-edge  : stack_spoof=${WRAITH_USE_STACK_SPOOF} hwbp=${WRAITH_USE_HWBP_HOOKS} priv_ntdll=${WRAITH_USE_PRIVATE_NTDLL}")
  message(STATUS "  threadless=${WRAITH_USE_THREADLESS_EXEC} page_guard=${WRAITH_USE_PAGE_GUARD_ENCRYPT}")
  message(STATUS "  heap_masq=${WRAITH_USE_HEAP_MASQUERADE} anti_dbg=${WRAITH_USE_ANTI_DEBUG_SPOOF} host_iat=${WRAITH_USE_HOST_IAT_REDIRECT}")
  message(STATUS "  Hardening  : cfg=${WRAITH_HARDEN_CFG} cet=${WRAITH_HARDEN_CET} stack=${WRAITH_HARDEN_STACK_PROTECT} no_crt=${WRAITH_HARDEN_NO_CRT}")
  message(STATUS "  Build artifacts : static=${WRAITH_BUILD_STATIC} shared=${WRAITH_BUILD_SHARED} tests=${WRAITH_BUILD_TESTS} examples=${WRAITH_BUILD_EXAMPLES}")
  message(STATUS "  fuzz=${WRAITH_BUILD_FUZZ} pic=${WRAITH_BUILD_PIC}")
  message(STATUS "====================================================================")
  message(STATUS "")
endfunction()
