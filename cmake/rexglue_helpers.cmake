#==========================================================
# rexglue_configure_target() - Configure a consumer target
# with platform-specific settings and SDK source files.
#
# Usage:
#   rexglue_configure_target(<target>)
#
# Adds:
#   - Platform entry point source (windowed_app_main_*.cpp)
#   - ReXApp base class source (rex_app.cpp)
#   - Platform-specific link/compile settings
#==========================================================
function (rexglue_configure_target target_name)
  set(rexglue_effective_system_processor "${REXGLUE_EFFECTIVE_SYSTEM_PROCESSOR}")
  if (NOT rexglue_effective_system_processor)
    set(rexglue_effective_system_processor "${CMAKE_SYSTEM_PROCESSOR}")
    if (APPLE AND CMAKE_OSX_ARCHITECTURES)
      list(GET CMAKE_OSX_ARCHITECTURES 0 rexglue_effective_system_processor)
    endif ()
  endif ()
  string(TOLOWER "${rexglue_effective_system_processor}" rexglue_effective_system_processor_lower)

  # Platform entry point
  if (WIN32)
    target_sources(${target_name} PRIVATE ${REXGLUE_SHARE_DIR}/windowed_app_main_win.cpp)
  else ()
    target_sources(${target_name} PRIVATE ${REXGLUE_SHARE_DIR}/windowed_app_main_posix.cpp)
  endif ()

  # ReXApp base class
  target_sources(${target_name} PRIVATE ${REXGLUE_SHARE_DIR}/rex_app.cpp)

  # Linux platform settings
  if (UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    target_include_directories(${target_name} PRIVATE ${GTK3_INCLUDE_DIRS})
    target_link_libraries(${target_name} PRIVATE ${GTK3_LIBRARIES})

    # Whole-archive linking for kernel hooks
    target_link_options(
      ${target_name} PRIVATE -Wl,--whole-archive $<TARGET_FILE:rex::kernel> -Wl,--no-whole-archive
    )
    # Large executable support
    if (rexglue_effective_system_processor_lower MATCHES "^(x86_64|amd64)$")
      target_link_options(${target_name} PRIVATE -Wl,--no-relax)
      target_compile_options(${target_name} PRIVATE -mcmodel=large)
    elseif (rexglue_effective_system_processor_lower MATCHES "^(aarch64|arm64)$")
      target_compile_options(${target_name} PRIVATE -march=armv8-a)
    endif ()
  endif ()

  if (NOT MSVC)
    if (NOT APPLE AND rexglue_effective_system_processor_lower MATCHES "^(x86_64|amd64)$")
      target_compile_options(${target_name} PRIVATE -msse4.1)
    endif ()
    # ARM64 NEON is enabled via -march=armv8-a above
  endif ()

  # Copy runtime DLLs next to the executable
  if (WIN32)
    add_custom_command(
      TARGET ${target_name}
      POST_BUILD
      COMMAND "$<$<BOOL:$<TARGET_RUNTIME_DLLS:${target_name}>>:${CMAKE_COMMAND};-E;copy_if_different;$<TARGET_RUNTIME_DLLS:${target_name}>;$<TARGET_FILE_DIR:${target_name}>>"
      COMMAND_EXPAND_LISTS
    )
    # FidelityFX is linked PRIVATE by rexui, so copy its DLLs explicitly.
    if (TARGET amd_fidelityfx_vk)
      add_custom_command(
        TARGET ${target_name}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:amd_fidelityfx_vk>
                $<TARGET_FILE_DIR:${target_name}>
      )
    endif ()
    if (TARGET amd_fidelityfx_dx12)
      add_custom_command(
        TARGET ${target_name}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:amd_fidelityfx_dx12>
                $<TARGET_FILE_DIR:${target_name}>
      )
    endif ()
  elseif (APPLE)
    target_link_options(${target_name} PRIVATE -Wl,-rpath,@executable_path)
    if (TARGET MoltenVK)
      add_custom_command(
        TARGET ${target_name}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:MoltenVK>
                $<TARGET_FILE_DIR:${target_name}>
      )
    endif ()
  endif ()
endfunction ()
