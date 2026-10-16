# MinGW x86 logic-verification build — NOT the project target

> **Current verdict, 2026-10-16:** the full tree (OpenSpore.exe + all 29 verify
> tools) compiles and links under 32-bit GCC with **zero repository source
> changes**, and every check passes except one environmental errno-mapping case.
> This verifies program *logic* on this machine. It does **not** replace the
> MSVC Win32 Debug target: no warning parity, no CRT-behavior parity, no
> performance claims. The day a machine with Visual Studio Build Tools appears,
> the MSVC build commands in [README](../README.md) are still the ones that count.

## Why this exists

No admin rights on this machine, so Visual Studio Build Tools cannot be installed.
A user-local WinLibs i686 GCC 16.1.0 (same vendor/version as the x86_64 toolchain
already present via winget) plus the already-present Ninja gives a 32-bit build
with the project's own `CMakeLists.txt` unmodified. Pointer widths, `u32`
address handling and struct layouts are x86-correct; a 64-bit build would not be.

## Layout (all gitignored)

* Compiler: `C:\Users\gelion\winlibs-i686` (unpacked WinLibs i686 zip, ~1 GB).
  From [winlibs.com](https://winlibs.com/):
  `winlibs-i686-posix-dwarf-gcc-16.1.0-mingw-w64ucrt-14.0.0-r4.zip`.
* Build tree: `build/mingw/` (separate from `build/bin/Debug/`).
* `build/mingw/bin/Debug/Data` and `DataEP1` are **junctions** to
  `build/bin/Debug/Data(DataEP1)` — no 5.7 GB copy.
* `build/mingw/toolchain-i686.cmake`, `mingw_shim.h`, `project-include.cmake`
  (contents retained below; the live copies stay out of git with the tree).

## The three adaptations (build system only, all outside the repo)

1. `mingw_shim.h`, force-included:
   `#ifndef _MSC_VER / #define _MSC_VER 0 / #endif` — the log's compiler tag
   then reads `MSVC 0`, which honestly marks a non-MSVC binary.
2. `project-include.cmake` (via `CMAKE_PROJECT_OpenSpore_INCLUDE`):
   `link_libraries(gdiplus uuid)` — MSVC gets these from `#pragma comment(lib)`,
   which GCC ignores, and link order puts them after the objects. Plus a
   `cmake_language(DEFER CALL …)` that adds `-municode` to the OpenSpore target
   only: verify tools link `int main` (broken by `-municode`), OpenSpore links
   `wWinMain` (broken without it).
3. Toolchain flags: `-finput-charset=UTF-8 -fexec-charset=UTF-8` (match `/utf-8`),
   `-fms-extensions` (Windows headers), `-fpermissive` (MinGW's own COM headers
   use `__uuidof` in template members, rejected by GCC ≥ 14 otherwise),
   `-mfpmath=sse -msse2 -ffp-contract=off` (match MSVC `/fp:precise` on SSE2).

## Results 2026-10-16 (346/346 build steps, i686 GCC 16.1.0)

Verify tools, exit 0 unless noted — spui 748/748, mode_ui, intro 2040, dbcheck,
catalog, starsdb 240, links 362, context 126, dbpf 266, postload 15, save_header
116, planet_record 291, present 50, instance 167, phase1 247, transition 195,
input 29, stages, pkg_lifetime 544, lifecycle 232, scene 204, image 80 (+ D3D9
readback), animated 159 (+ GPU readback), overlay_clip (+ scissor readback).

App runs (each exit 0, aborts/asserts 0): headless `state:-list` (10/10 boot
markers), `-state:CellGame` (skeleton driver active in the log + 1280x720
capture), DevLauncher + capture, visible click → `CreatureEditor` chain,
Main → GalaxyGameEntry + capture, GGEHostPreview click → `picked host card 20481`.

## Two deviations, both root-caused, neither touching product logic

1. **x87 vs SSE2 float math.** `gge_star_instance_verify` failed first run
   (`placed_area` exact-equality check): i686 GCC defaults to x87 excess
   precision, MSVC uses SSE2. Fixed in the build config (`-mfpmath=sse -msse2
   -ffp-contract=off`); then 167/167 PASS. Lesson recorded: the engine's exact
   float checks make `/fp:precise` load-bearing, not cosmetic — the comment in
   `CMakeLists.txt` already says so, and this is independent confirmation.
2. **Truncate-while-open errno mapping.** `gge_streaming_image_verify` expects
   `ERROR_SHARING_VIOLATION` (32) when truncating a file the provider holds open;
   under UCRT/MinGW the same operation yields EACCES (13) with the file likewise
   unchanged. Standalone probes (`shareprobe.c`, `shareprobe2.cpp` — since
   removed) confirmed: protection holds, only the error *code* differs. The
   MSVC expectation is untouched; this is CRT/STL mapping, not engine logic.

## Reproduce

```powershell
$env:PATH = 'C:\Users\gelion\winlibs-i686\mingw32\bin;<ninja-dir>;' + $env:PATH
cmake -S . -B build/mingw -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=C:/Users/gelion/Desktop/OpenSpore/build/mingw/toolchain-i686.cmake `
  -DCMAKE_PROJECT_OpenSpore_INCLUDE=C:/Users/gelion/Desktop/OpenSpore/build/mingw/project-include.cmake `
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/mingw --target OpenSpore <verify targets...>  # full list in README
# run from build/mingw/bin/Debug with i686 bin on PATH; GUI exes need Start-Process -Wait
```

Toolchain file:

```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR X86)

set(WINLIBS_I686 "C:/Users/gelion/winlibs-i686/mingw32/bin")
set(CMAKE_C_COMPILER "${WINLIBS_I686}/gcc.exe")
set(CMAKE_CXX_COMPILER "${WINLIBS_I686}/g++.exe")
set(CMAKE_RC_COMPILER "${WINLIBS_I686}/windres.exe")

set(SHIM "C:/Users/gelion/Desktop/OpenSpore/build/mingw/mingw_shim.h")
set(CMAKE_C_FLAGS_INIT "-finput-charset=UTF-8 -fexec-charset=UTF-8 -fms-extensions")
set(CMAKE_CXX_FLAGS_INIT "-finput-charset=UTF-8 -fexec-charset=UTF-8 -fms-extensions -fpermissive -mfpmath=sse -msse2 -ffp-contract=off -include ${SHIM}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "")
```

`project-include.cmake`:

```cmake
link_libraries(gdiplus uuid)
cmake_language(DEFER CALL _mingw_fix_openspore_entry)
function(_mingw_fix_openspore_entry)
  if(TARGET OpenSpore)
    target_link_options(OpenSpore PRIVATE -municode)
  endif()
endfunction()
```

`mingw_shim.h`: `#pragma once` + the `_MSC_VER 0` guard quoted above.

## Boundaries

* MSVC warnings are unchecked here by construction; a MinGW-clean file can still
  warn under `/W4`. First MSVC build must still be treated as unverified ground.
* UCRT vs MSVCRT differences (sharing modes, errno mapping, dialog behavior)
  are environmental; anything depending on them needs an MSVC re-run.
* No FPS or load-time numbers are recorded — this machine's build is for logic,
  and Debug numbers would mislead anyway.
