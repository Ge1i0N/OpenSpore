#include "engine/appmodes.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "engine/appstates.h"

namespace os::appmodes {
namespace {

// THE TABLE. Read out of the binary, one row per registration call. See analysis/modes.md for the
// decompiled evidence behind each address; nothing here is inferred from a name looking plausible.
//
// tag          the marker address the registrar passes as the mode's identity
// registrar    the function containing the `(**(code**)(*reg + 0x20))(obj, tag, "name")`
// object_size  the size the factory allocates before constructing (4 = a bare vtable pointer)
// state        the state whose shipped script asks for this mode - the cross-validation column
const Mode kModes[] = {
    {"Game_Cell",         0x01654C00u, 0x00e616c0u, 0x0cu, "CellGame"},
    {"Game_Creature",     0x01654C01u, 0x00d3bd20u, 0xe8u, "CreatureGame"},
    {"Game_Tribe",        0x01654C02u, 0x00cdb4e0u, 0x3a8u, "TribeGame"},
    {"Game_Civ",          0x01654C04u, 0x00cfa480u, 0x150u, "CivilisationGame"},
    {"Game_Space",        0x01654C05u, 0x00fddb30u, 0x198u, "SpaceGame"},
    {"Game_EditMode",     0x01654C06u, 0x00d1c2a0u, 0x54u, "LevelEditor"},
    {"LoadGame",          0x01654C08u, 0x00b5d500u, 0x04u, "LoadGame"},
    {"ScenarioMode",      0x01654C10u, 0x00ef1870u, 0xe0u, "ScenarioMode"},
    {"GalaxyGameEntry",   0x02CCD1D2u, 0x00ddde30u, 0x10u, "GalaxyGameEntry"},
    {"GameCinematics",    0x8916F92Du, 0x00eb2e40u, 0x48u, "GameCinematicsMode"},
};

// The script asks for 17 modes; the table above has 10. The other 7 are NOT all the same kind of
// missing, and conflating them is how a coverage gap turns into a wrong claim. Measured
// (tools/mode_where.py, tools/mode_absent.py, both searching the whole 24.9 MB oracle):
//   * five appear NOWHERE, not even as a substring, in ASCII or UTF-16 - the script asks for classes
//     this build does not ship;
//   * two DO occur in the binary (`TerrainEditor` 5 times, `Editors` once) but their registration was
//     not located, because they are not defined STRING data and so `ghidra_xref.py --string` cannot
//     reach them. Those two are a REAL, ACTIONABLE gap: two registration calls still to find, which
//     means the mode table is 10 of a likely 12.
struct Absent { const char* name; bool in_binary; };
const Absent kNotInTable[] = {
    {"AnimViewer", false}, {"EffectsViewer", false}, {"Launcher", false}, {"ModelViewer", false},
    {"MultiverseJournal", false},
    {"TerrainEditor", true}, {"Editors", true},
};

int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

bool ieq(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (lower((unsigned char)*a) != lower((unsigned char)*b)) return false;
        ++a;
        ++b;
    }
    return *a == *b;
}

// FUN_00b5dcd0's own switch, transcribed. Kept beside the table so the two can be read together:
// if a tag is confirmed to drive another state, this is where the correction goes.
struct TagState { u32 tag; const char* state; };
const TagState kTagState[] = {
    {0x01654C00u, "CellGame"},
    {0x01654C01u, "CreatureGame"},
    {0x01654C02u, "TribeGame"},
    {0x01654C04u, "CivilisationGame"},
    {0x01654C05u, "SpaceGame"},
    {0x01654C10u, "ScenarioMode"},
    {0x02CCD1D2u, "GalaxyGameEntry"},
};

}  // namespace

const Mode* table() { return kModes; }
size_t count() { return sizeof(kModes) / sizeof(kModes[0]); }

const Mode* find_by_name(const char* name) {
    if (!name || !*name) return nullptr;
    for (const Mode& m : kModes) {
        if (ieq(m.name, name)) return &m;
    }
    return nullptr;
}

const char* state_for_tag(u32 tag) {
    for (const TagState& t : kTagState) {
        if (t.tag == tag) return t.state;
    }
    return nullptr;
}

std::string describe(const Mode& m) {
    char b[220];
    std::snprintf(b, sizeof(b),
                  "mode %-16s tag=0x%08X registered by 0x%08X (obj %u B) -> state '%s'",
                  m.name, m.tag, m.registrar, m.object_size, m.state);
    return b;
}

std::string report() {
    char b[240];
    std::string rows;
    for (const Mode& m : kModes) {
        std::snprintf(b, sizeof(b),
                      "\n  %-16s tag=0x%08X  reg=0x%08X  obj=%3u B  state='%s'", m.name, m.tag,
                      m.registrar, m.object_size, m.state);
        rows += b;
    }
    // COVERAGE, measured rather than asserted: the script asks for modes by name; a name it uses that
    // is not in the table is split into "this build has no such class" and "the class exists but its
    // registration is not located yet" - the second is a TO-DO, the first is a fact about the build.
    std::vector<std::string> seen_names;
    size_t asked = 0, in_table = 0, absent = 0, unfound = 0;
    std::string missing_absent, missing_unfound;
    auto& tb = appstates::instance();
    if (tb.loaded()) {
        for (const auto& gs : tb.states()) {
            if (gs.mode.empty()) continue;
            bool dup = false;
            for (const std::string& s : seen_names) {
                if (s == gs.mode) { dup = true; break; }
            }
            if (dup) continue;
            seen_names.push_back(gs.mode);
            ++asked;
            if (find_by_name(gs.mode.c_str())) {
                ++in_table;
                continue;
            }
            bool in_binary = false;
            for (const Absent& a : kNotInTable) {
                if (ieq(a.name, gs.mode.c_str())) { in_binary = a.in_binary; break; }
            }
            if (in_binary) {
                ++unfound;
                missing_unfound += "\n    " + gs.mode;
            } else {
                ++absent;
                missing_absent += "\n    " + gs.mode;
            }
        }
    }
    // VERDICT FIRST. A long multi-line console message is cut by the log's per-message buffer, and the
    // list below is regenerable while the conclusion is not - learned the hard way on `spui -ctl`.
    std::string out;
    std::snprintf(b, sizeof(b),
                  "%zu modes recovered from the binary; the script asks for %zu distinct mode(s): "
                  "%zu in the table, %zu absent from this build, %zu present but registration unfound",
                  count(), asked, in_table, absent, unfound);
    out = b;
    if (!missing_absent.empty()) {
        out += "\n  asked for but ABSENT from this build (nowhere in the 24.9 MB oracle, not even a"
               " substring):" + missing_absent;
    }
    if (!missing_unfound.empty()) {
        out += "\n  asked for, PRESENT in the binary, registration NOT YET FOUND (no defined string"
               " data, so an xref-by-text cannot reach it):" + missing_unfound;
    }
    out += "\n  table (each row is one registration call in the binary):" + rows;
    return out;
}

}  // namespace os::appmodes
