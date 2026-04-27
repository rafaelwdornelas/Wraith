# tests/integration/CMakeLists.txt
#
# Integration tests. Each test executable links against the library
# target and is registered with ctest under wine64 via
# wr_add_wine_test.

# --- Fixtures ---------------------------------------------------------
add_subdirectory(fixtures/payload_dll)
add_subdirectory(fixtures/forwarder_dll)
add_subdirectory(fixtures/seh_dll)

# Helper that wires up a test executable that loads the payload_dll
# fixture: link library, set suffix, depend on the fixture, and copy
# `payload.dll` next to the test exe so wine can find it via the
# default relative path.
function(_wraith_payload_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE wraith)
  target_include_directories(${name} PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_BINARY_DIR}/generated
  )
  if(NOT MSVC)
  set_target_properties(${name} PROPERTIES SUFFIX ".exe")
  endif()
  add_dependencies(${name} payload_dll)
  add_custom_command(TARGET ${name} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  $<TARGET_FILE:payload_dll>
  $<TARGET_FILE_DIR:${name}>/payload.dll
  )
endfunction()

# --- Trivial load ----------------------------------------------------
_wraith_payload_test(test_load_trivial test_load_trivial.c)
wr_add_wine_test(integration.load_trivial test_load_trivial)

# --- Forwarded export resolution -------------------------------------
add_executable(test_load_forwarder test_load_forwarder.c)
target_link_libraries(test_load_forwarder PRIVATE wraith)
target_include_directories(test_load_forwarder PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)
if(NOT MSVC)
  set_target_properties(test_load_forwarder PROPERTIES SUFFIX ".exe")
endif()
add_dependencies(test_load_forwarder forwarder_dll)
add_custom_command(TARGET test_load_forwarder POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  $<TARGET_FILE:forwarder_dll>
  $<TARGET_FILE_DIR:test_load_forwarder>/forwarder_dll.dll
)
wr_add_wine_test(integration.load_forwarder test_load_forwarder)

# --- SEH x64 registration + DllMain DETACH symmetry ------------------
add_executable(test_load_seh test_load_seh.c)
target_link_libraries(test_load_seh PRIVATE wraith)
target_include_directories(test_load_seh PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)
if(NOT MSVC)
  set_target_properties(test_load_seh PROPERTIES SUFFIX ".exe")
endif()
add_dependencies(test_load_seh seh_dll)
add_custom_command(TARGET test_load_seh POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  $<TARGET_FILE:seh_dll>
  $<TARGET_FILE_DIR:test_load_seh>/seh_dll.dll
)
wr_add_wine_test(integration.load_seh test_load_seh)

# --- Hashed API resolution (PEB walk + DJB2) -------------------------
if(WRAITH_USE_API_HASHING)
  _wraith_payload_test(test_load_hashed test_load_hashed.c)
  wr_add_wine_test(integration.load_hashed test_load_hashed)
endif()

# --- Anti-debug + heap masquerade + host IAT redirect ----------------
if(WRAITH_USE_HEAP_MASQUERADE OR WRAITH_USE_ANTI_DEBUG_SPOOF OR WRAITH_USE_HOST_IAT_REDIRECT)
  add_executable(test_anti_debug_heap_iat test_anti_debug_heap_iat.c)
  target_link_libraries(test_anti_debug_heap_iat PRIVATE wraith)
  target_include_directories(test_anti_debug_heap_iat PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
  )
  if(NOT MSVC)
  set_target_properties(test_anti_debug_heap_iat PROPERTIES SUFFIX ".exe")
  endif()
  wr_add_wine_test(integration.anti_debug_heap_iat test_anti_debug_heap_iat)
endif()

# --- Mockingjay mapping ----------------------------------------------
_wraith_payload_test(test_load_mockingjay test_load_mockingjay.c)
wr_add_wine_test(integration.load_mockingjay test_load_mockingjay)

# --- Page-Guard self-encryption --------------------------------------
if(WRAITH_USE_PAGE_GUARD_ENCRYPT)
  _wraith_payload_test(test_page_guard test_page_guard.c)
  wr_add_wine_test(integration.page_guard test_page_guard)
endif()

# --- Threadless execution --------------------------------------------
if(WRAITH_USE_THREADLESS_EXEC)
  add_executable(test_threadless test_threadless.c)
  target_link_libraries(test_threadless PRIVATE wraith)
  target_include_directories(test_threadless PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
  )
  if(NOT MSVC)
  set_target_properties(test_threadless PROPERTIES SUFFIX ".exe")
  endif()
  wr_add_wine_test(integration.threadless test_threadless)
endif()

# --- FreshyCalls SSN-by-RVA ------------------------------------------
if(WRAITH_USE_INDIRECT_SYSCALLS AND WRAITH_USE_API_HASHING)
  add_executable(test_freshycalls test_freshycalls.c)
  target_link_libraries(test_freshycalls PRIVATE wraith)
  target_include_directories(test_freshycalls PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
  )
  if(NOT MSVC)
  set_target_properties(test_freshycalls PROPERTIES SUFFIX ".exe")
  endif()
  wr_add_wine_test(integration.freshycalls test_freshycalls)
endif()

# --- Private ntdll mapping -------------------------------------------
if(WRAITH_USE_PRIVATE_NTDLL)
  add_executable(test_private_ntdll test_private_ntdll.c)
  target_link_libraries(test_private_ntdll PRIVATE wraith)
  target_include_directories(test_private_ntdll PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
  )
  if(NOT MSVC)
  set_target_properties(test_private_ntdll PROPERTIES SUFFIX ".exe")
  endif()
  wr_add_wine_test(integration.private_ntdll test_private_ntdll)
endif()

# --- Cronos sleep ----------------------------------------------------
if(WRAITH_USE_SLEEP_OBFUSCATION)
  _wraith_payload_test(test_load_sleep_cronos test_load_sleep_cronos.c)
  wr_add_wine_test(integration.load_sleep_cronos test_load_sleep_cronos)
endif()

# --- Hardware-breakpoint hooks ---------------------------------------
if(WRAITH_USE_HWBP_HOOKS)
  add_executable(test_hwbp test_hwbp.c)
  target_link_libraries(test_hwbp PRIVATE wraith)
  target_include_directories(test_hwbp PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  )
  if(NOT MSVC)
  set_target_properties(test_hwbp PROPERTIES SUFFIX ".exe")
  endif()
  wr_add_wine_test(integration.hwbp test_hwbp)
endif()

# --- Stack spoofing --------------------------------------------------
if(WRAITH_USE_STACK_SPOOF AND WRAITH_USE_API_HASHING AND WRAITH_USE_INDIRECT_SYSCALLS)
  _wraith_payload_test(test_load_stack_spoof test_load_stack_spoof.c)
  wr_add_wine_test(integration.load_stack_spoof test_load_stack_spoof)
endif()

# --- ETW + AMSI patches ----------------------------------------------
if(WRAITH_USE_ETW_PATCH OR WRAITH_USE_AMSI_PATCH)
  add_executable(test_etw_amsi_patch test_etw_amsi_patch.c)
  target_link_libraries(test_etw_amsi_patch PRIVATE wraith)
  target_include_directories(test_etw_amsi_patch PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
  )
  if(NOT MSVC)
  set_target_properties(test_etw_amsi_patch PROPERTIES SUFFIX ".exe")
  endif()
  wr_add_wine_test(integration.etw_amsi_patch test_etw_amsi_patch)
endif()

# --- Userland unhooking (ntdll disk refresh) -------------------------
if(WRAITH_USE_UNHOOK_NTDLL)
  add_executable(test_unhook test_unhook.c)
  target_link_libraries(test_unhook PRIVATE wraith)
  target_include_directories(test_unhook PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
  )
  if(NOT MSVC)
  set_target_properties(test_unhook PROPERTIES SUFFIX ".exe")
  endif()
  wr_add_wine_test(integration.unhook test_unhook)
endif()

# --- Module Stomping -------------------------------------------------
if(WRAITH_USE_MODULE_STOMPING)
  _wraith_payload_test(test_load_module_stomping test_load_module_stomping.c)
  wr_add_wine_test(integration.load_module_stomping test_load_module_stomping)
endif()

# --- Sleep obfuscation -----------------------------------------------
if(WRAITH_USE_SLEEP_OBFUSCATION)
  _wraith_payload_test(test_load_sleep_obf test_load_sleep_obf.c)
  wr_add_wine_test(integration.load_sleep_obf test_load_sleep_obf)
endif()

# --- PEB.Ldr linkage with masquerade ---------------------------------
if(WRAITH_USE_PEB_LINKAGE)
  add_executable(test_load_peb_linkage test_load_peb_linkage.c)
  target_link_libraries(test_load_peb_linkage PRIVATE wraith psapi)
  target_include_directories(test_load_peb_linkage PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  )
  if(NOT MSVC)
  set_target_properties(test_load_peb_linkage PROPERTIES SUFFIX ".exe")
  endif()
  add_dependencies(test_load_peb_linkage payload_dll)
  add_custom_command(TARGET test_load_peb_linkage POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  $<TARGET_FILE:payload_dll>
  $<TARGET_FILE_DIR:test_load_peb_linkage>/payload.dll
  )
  wr_add_wine_test(integration.load_peb_linkage test_load_peb_linkage)
endif()

# --- Phantom DLL Hollowing -------------------------------------------
if(WRAITH_USE_PHANTOM_HOLLOWING AND WRAITH_USE_INDIRECT_SYSCALLS)
  _wraith_payload_test(test_load_phantom test_load_phantom.c)
  wr_add_wine_test(integration.load_phantom test_load_phantom)
endif()

# --- Hell's Hall indirect syscalls -----------------------------------
if(WRAITH_USE_INDIRECT_SYSCALLS AND WRAITH_USE_API_HASHING)
  _wraith_payload_test(test_load_syscalls test_load_syscalls.c)
  wr_add_wine_test(integration.load_syscalls test_load_syscalls)
endif()
