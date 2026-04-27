# examples/full_stealth_chain/CMakeLists.txt
#
# Showcase consumer that exercises every classic-tier stealth flag
# in one binary. Built only when the corresponding library options
# are enabled.

if(WRAITH_USE_API_HASHING
  AND WRAITH_USE_INDIRECT_SYSCALLS
  AND WRAITH_USE_PHANTOM_HOLLOWING
  AND WRAITH_USE_PEB_LINKAGE
  AND WRAITH_USE_SLEEP_OBFUSCATION)

  add_executable(full_stealth_chain main.c)
  target_link_libraries(full_stealth_chain PRIVATE wraith)
  target_include_directories(full_stealth_chain PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  )
  if(NOT MSVC)
  set_target_properties(full_stealth_chain PROPERTIES SUFFIX ".exe")
  endif()
  add_dependencies(full_stealth_chain payload_dll)
  add_custom_command(TARGET full_stealth_chain POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  $<TARGET_FILE:payload_dll>
  $<TARGET_FILE_DIR:full_stealth_chain>/payload.dll
  )
endif()
