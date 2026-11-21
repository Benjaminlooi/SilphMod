cmake_minimum_required(VERSION 3.22.1)
project(SilphEditor LANGUAGES C)

# Android Studio's code model only. Keep sources/definitions aligned with
# tools/build_native.ps1, which remains responsible for the shipped binaries.
set(SILPH_IDE_MODE "farmer" CACHE STRING "Native mode used by Android Studio")
set_property(CACHE SILPH_IDE_MODE PROPERTY STRINGS farmer kecleon rocket enhancer)
if(NOT SILPH_IDE_MODE MATCHES "^(farmer|kecleon|rocket|enhancer)$")
    message(FATAL_ERROR "Unknown SILPH_IDE_MODE: ${SILPH_IDE_MODE}")
endif()

set(common_sources
    unity-pilot.c managed.c managed-read.c game-read.c scheduler.c control.c
    promise.c managed-promise.c farmer-actions.c transfer-policy.c rpc-actions.c
    kecleon-actions.c maintenance-actions.c farmer-session.c session-wire.c
    session-link.c kecleon-wire.c kecleon-plan.c kecleon-session.c visual-tools.c
    research-policy.c research-actions.c location-tools.c code-memory.c
    presentation-tools.c buddy-actions.c recovery-policy.c readback.c core.c
    startup-thread.c callback-memory.c gift-actions.c pass-actions.c)
set(common_definitions
    SILPH_PILOT_FRAME=1 SILPH_PRESENT_UNWIND=1 SILPH_PILOT_GAME_READ=1
    SILPH_PILOT_EXTERNAL=1 SILPH_PILOT_ACTIONS=1 SILPH_ENABLE_ACTIONS=1
    SILPH_PILOT_CONFIGURED=1 SILPH_PILOT_SERVICE=1)
set(mode_sources farmer-view.c)
if(SILPH_IDE_MODE STREQUAL "farmer")
    set(mode_definitions SILPH_PILOT_TRANSFER=1 SILPH_PILOT_CONTINUOUS=1
        SILPH_PILOT_COMBINED=1 SILPH_PRESENT_SCOPED=1)
    list(APPEND mode_sources event-actions.c buddy-care.c)
elseif(SILPH_IDE_MODE STREQUAL "enhancer")
    set(mode_definitions SILPH_ENABLE_ENHANCER=1 SILPH_PILOT_ENHANCER=1
        SILPH_PILOT_ROCKET=1 SILPH_PILOT_COMBINED=1 SILPH_PRESENT_SCOPED=1
        SILPH_PILOT_AUTORAID=1)
    list(APPEND mode_sources rocket-policy.c rocket-session.c rocket-flow.c
        enhancer-tools.c buddy-care.c raid-read.c raid-pass.c raid-policy.c)
elseif(SILPH_IDE_MODE STREQUAL "rocket")
    set(mode_definitions SILPH_PILOT_ROCKET=1 SILPH_PILOT_COMBINED=1)
    list(APPEND mode_sources rocket-policy.c rocket-session.c rocket-flow.c)
else()
    set(mode_definitions SILPH_PILOT_KECLEON=1)
endif()

set(SILPH_SHADOWHOOK "$ENV{SILPH_SHADOWHOOK}" CACHE PATH "Existing ShadowHook checkout")
if(NOT SILPH_SHADOWHOOK)
    set(SILPH_SHADOWHOOK "${CMAKE_CURRENT_SOURCE_DIR}/../.dependencies/shadowhook")
endif()

function(editor_target target)
    add_library(${target} OBJECT EXCLUDE_FROM_ALL ${ARGN})
    set_target_properties(${target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO POSITION_INDEPENDENT_CODE YES)
    target_compile_options(${target} PRIVATE -O2 -Wall -Wextra -Werror
        -fvisibility=hidden -fstack-protector-strong)
    target_compile_definitions(${target} PRIVATE _FORTIFY_SOURCE=2)
endfunction()

editor_target(silph-${SILPH_IDE_MODE}-editor ${common_sources} ${mode_sources})
target_compile_definitions(silph-${SILPH_IDE_MODE}-editor PRIVATE
    ${common_definitions} ${mode_definitions})
target_compile_options(silph-${SILPH_IDE_MODE}-editor PRIVATE -fexceptions -pthread)
target_include_directories(silph-${SILPH_IDE_MODE}-editor PRIVATE
    "${SILPH_SHADOWHOOK}/shadowhook/src/main/cpp/include")

editor_target(silph-status-editor pilot-reader.c standalone/remote.c standalone/elf-symbol.c)
target_compile_definitions(silph-status-editor PRIVATE ${common_definitions} ${mode_definitions})
target_compile_options(silph-status-editor PRIVATE -fexceptions)
editor_target(silph-control-editor control-client.c control.c scheduler.c session-wire.c kecleon-wire.c)
target_compile_definitions(silph-control-editor PRIVATE SILPH_ENABLE_ENHANCER=1)
editor_target(silph-probe-editor standalone/probe.c standalone/remote.c standalone/elf-symbol.c)
target_compile_definitions(silph-probe-editor PRIVATE SILPH_LEGACY_HWSTEP=1)

# Gradle selects this no-op target so indexing does not duplicate native builds.
add_custom_target(silph-ide)
