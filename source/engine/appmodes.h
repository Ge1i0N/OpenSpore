#pragma once
// THE MODE TABLE, as extracted from the binary. Ten rows; see analysis/modes.md for the evidence.
//
// A `mode` is NOT a data table we could load (that was the assumption, and it was wrong): it is a
// named driver CLASS, registered by a per-class factory under a TAG, and selected by that tag. The
// whole table is ten calls of the shape
//
//     (**(code **)(*<registry> + 0x20))(object, <TAG>, "<NAME>");
//
// which is why this header exists as a table of addresses rather than a parser: the deliverable that
// is honestly available to us now is that the engine KNOWS the table - the tag, the registering
// function, the object size, and the state the script pairs the mode with - instead of printing
// "the engine's mode table is not implemented" with no idea what it is missing.
//
// Each row is cited to its registering function in refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe.
#include <string>

#include "common.h"

namespace os::appmodes {

// The MODE TAG is the address of a static zero-initialised marker in .data; its IDENTITY is its
// address, not its value (the markers sit one byte apart because the linker packed empty statics).
// A hole in the sequence (no 03, no 07) is a marker placed elsewhere, not a missing mode.
struct Mode {
    const char* name;      // the literal the registrar passes - the game's own spelling
    u32 tag;               // the marker address used as the mode's identity
    u32 registrar;         // the function that allocates, constructs and registers it
    u32 object_size;       // bytes the factory allocates (4 = a bare vtable)
    const char* state;     // the state whose own script asks for this mode (cross-validated)
};

// All ten, in tag order of the .data block. Registering addresses are the disassembly's own.
const Mode* table();
size_t count();

// The mode a state's script names, or null. Matching is case-insensitive on the name, because the
// script and the binary agree in spelling but not necessarily in case across builds.
const Mode* find_by_name(const char* name);

// Which state a MODE TAG drives, per the original's own mapper FUN_00b5dcd0, or null. This is the
// third independent function that agrees on the tag space (registrations, the script, and the
// mapper), so the mapping here is the game's, not our guess.
const char* state_for_tag(u32 tag);

// Bounded mode-owned UI subset recovered from GA code, not inferred from a layout filename.
// This does NOT implement the original mode driver, galaxy renderer or widget event handlers.
struct UiBinding {
    const char* mode;
    const char* layout;
    u32 instance;
    u32 mount_control;     // loader's parent lookup operand; original attachment not implemented
    u32 load_function;
    u32 name_initializer;
    const char* limitation;
};
const UiBinding* primary_ui(const char* mode);

// A one-line description for the log: name, tag, registrar and the state it drives.
std::string describe(const Mode& m);

// A multi-line report for `gamestates -modes`: every mode with its citation, plus the count of
// modes the script asks for that are NOT in this table (which is the honest coverage gap).
std::string report();

}  // namespace os::appmodes
