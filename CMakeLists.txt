cmake_minimum_required(VERSION 3.25)
project(OpenSpore LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Target shape, decided by evidence, not taste:
#   * The original (refs/bins/ReleaseGA/SporeApp.exe) is 32-bit (machine 0x14c),
#     subsystem Windows GUI, image base 0x00400000, ASLR-capable. PROMPT.md asks for
#     x86 first, x64 later -> so x86 is the default and the file is written to allow
#     a 64-bit pass without restructuring.
#   * Debug is the shipped configuration of this project (developer commands must be
#     present), so the default build type is Debug rather than Release.
# ---------------------------------------------------------------------------

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

set(OS_ARCH "x86" CACHE STRING "x86 or x64 (x86 first, per PROMPT.md)")
set_property(CACHE OS_ARCH PROPERTY STRINGS x86 x64)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(OS_IS_X64 TRUE)
else()
  set(OS_IS_X64 FALSE)
endif()
if(OS_ARCH STREQUAL "x64" AND NOT OS_IS_X64)
  message(FATAL_ERROR "OS_ARCH=x64 but the generator is 32-bit. Use -A x64.")
endif()
if(OS_ARCH STREQUAL "x86" AND OS_IS_X64)
  message(FATAL_ERROR "OS_ARCH=x86 but the generator is 64-bit. Use -A Win32.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep the icon out of any glob: it is generated into the build dir, never into source/.
set(OS_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${OS_GENERATED_DIR})

# ---------------------------------------------------------------------------
# Icon. The tracked copy is assets/OpenSpore.ico; refs/ is gitignored (it holds the
# original binaries), so it is only a fallback for the very first checkout.
# ---------------------------------------------------------------------------
set(OS_ICON_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/assets/OpenSpore.ico)
if(NOT EXISTS ${OS_ICON_SOURCE})
  set(OS_ICON_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/refs/OpenSpore.ico)
endif()
set(OS_ICON_BUILD  ${OS_GENERATED_DIR}/openspore.ico)
set(OS_RC_FILE     ${OS_GENERATED_DIR}/openspore.rc)
set(OS_HAS_ICON FALSE)
if(EXISTS ${OS_ICON_SOURCE})
  configure_file(${OS_ICON_SOURCE} ${OS_ICON_BUILD} COPYONLY)
  set(OS_HAS_ICON TRUE)
  message(STATUS "OpenSpore: icon from ${OS_ICON_SOURCE}")
endif()
if(OS_HAS_ICON)
  file(WRITE ${OS_RC_FILE}
"#include <windows.h>
#define OS_ICON_NAME 101
OS_ICON_NAME ICON \"openspore.ico\"
VS_VERSION_INFO VERSIONINFO
 FILEVERSION 3,1,0,29
 PRODUCTVERSION 3,1,0,29
 FILEFLAGSMASK 0x3fL
#ifdef _DEBUG
 FILEFLAGS 0x1L
#else
 FILEFLAGS 0x0L
#endif
 FILEOS 0x40004L
 FILETYPE 0x1L
 FILESUBTYPE 0x0L
BEGIN
    BLOCK \"StringFileInfo\"
    BEGIN
        BLOCK \"040904b0\"
        BEGIN
            VALUE \"CompanyName\", \"OpenSpore\"
            VALUE \"FileDescription\", \"OpenSpore Loader\"
            VALUE \"FileVersion\", \"3.1.0.29\"
            VALUE \"InternalName\", \"OpenSpore\"
            VALUE \"LegalCopyright\", \"Reimplementation; no EA content\"
            VALUE \"OriginalFilename\", \"OpenSpore.exe\"
            VALUE \"ProductName\", \"OpenSpore\"
            VALUE \"ProductVersion\", \"3.1.0.29\"
        END
    END
    BLOCK \"VarFileInfo\"
    BEGIN
        VALUE \"Translation\", 0x409, 1200
    END
END
")
else()
  message(WARNING "no OpenSpore.ico in assets/ or refs/ - building WITHOUT icon. "
                  "This is not a failure, but the taskbar/exe icon will be the default. "
                  "Tracked copy belongs in assets/ (refs/ is gitignored).")
  file(WRITE ${OS_RC_FILE} "// no icon: neither assets/OpenSpore.ico nor refs/OpenSpore.ico existed\n")
endif()

enable_language(RC)

# ---------------------------------------------------------------------------
# Sources. Explicit list, no globs (field guide §2: a glob once pulled a vendored
# library's .hlsl into the target and broke the build).
# ---------------------------------------------------------------------------
set(OS_SOURCES
  source/main.cpp
  source/app.cpp
  source/engine/diag.cpp
  source/engine/log.cpp
  source/engine/cmdline.cpp
  source/engine/fs.cpp
  source/engine/console.cpp
  source/engine/pkg.cpp
  source/engine/refpack.cpp
  source/engine/naming.cpp
  source/engine/gmdl.cpp
  source/engine/raster.cpp
  source/engine/config.cpp
  source/engine/appstates.cpp
  source/engine/spui.cpp
  source/engine/text.cpp
  source/render/device.cpp
  source/render/overlay.cpp
  source/render/imgfile.cpp
  source/render/mesh.cpp
  source/states/statemgr.cpp
  source/states/loader.cpp
  source/states/title.cpp
  source/states/uipresent.cpp
)

add_executable(OpenSpore WIN32 ${OS_SOURCES} ${OS_RC_FILE})
target_include_directories(OpenSpore PRIVATE source ${OS_GENERATED_DIR})

set(OS_DEV_COMMANDS OFF)
if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug" OR CMAKE_CONFIGURATION_TYPES)
  set(OS_DEV_COMMANDS ON)
endif()

target_compile_definitions(OpenSpore PRIVATE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  UNICODE
  _UNICODE
  $<$<BOOL:${OS_DEV_COMMANDS}>:OS_ENABLE_DEV_COMMANDS=1>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>   # keep Debug fast enough to tick a renderer
)

if(MSVC)
  target_compile_options(OpenSpore PRIVATE
    /W4 /permissive- /Zc:__cplusplus /utf-8 /EHsc /Zi /Gy-
    $<$<CONFIG:Debug>:/Od>
    /fp:precise          # the original is a 2008-era link; do not let fast-math vary results
    /Zc:inline-
  )
  target_link_options(OpenSpore PRIVATE /DEBUG /INCREMENTAL:NO /OPT:NOREF /OPT:NOICF)
  # /SAFESEH cannot be used with /INCREMENTAL and is irrelevant for a 32-bit GUI exe here.
  if(OS_ARCH STREQUAL "x86")
    target_link_options(OpenSpore PRIVATE /SAFESEH:NO)
  endif()
endif()

# d3d9 + the Win32 surface the original itself imports (see analysis/recon.md §Imports).
# NOTE: no d3dx9 - the June 2010 DX SDK is not installed here and we call no D3DX function.
# The original imports 9 d3dx9_27 functions; that dependency returns with the RW bridge.
target_link_libraries(OpenSpore PRIVATE
  d3d9
  user32 gdi32 kernel32 advapi32 shell32 ole32
  dbghelp
  winmm
)

set_target_properties(OpenSpore PROPERTIES
  OUTPUT_NAME "OpenSpore"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

message(STATUS "OpenSpore: arch=${OS_ARCH} build=${CMAKE_BUILD_TYPE} dev_commands=${OS_DEV_COMMANDS} icon=${OS_HAS_ICON}")
