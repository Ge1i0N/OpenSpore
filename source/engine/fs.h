#pragma once
// Filesystem search paths, per the reversed devdirs contract.
//
// Evidence (level 1): the startup function at 0x007e7e00..0x007e8420 builds its search
// list from these UTF-16 prefixes, all in .rdata:
//   'UserData/' 0x01413c8c   'BaseData/' 0x01413cb0   'Locale/' 0x01413cc4
//   'Config/'   0x01413ca0   'Data/'     0x01413cdc   './'      0x01413cd4
//   '../UserData/' 0x01413c58  'BaseData' 0x01413ce8  'My Spore Creations/' 0x01413b14
// keys: devDirs 0x013f8dc8  shipDirs 0x013f8db4  dataDir 0x01413cfc  userDataDir 0x01413c74
//       dataBaseDir 0x01413c40  locale 0x01403e18  writeableData 0x01413d3c
//
// Semantics we reproduce: [obj+0x159] useDevDirs = devDirs && !shipDirs (shipDirs wins).
// Dev mode puts loose directories first; ship mode resolves everything through packages.
// Package (.package.big) reading is NOT implemented yet - for now we resolve loose files
// and log every miss with the full search list, which is exactly the diagnostic the
// field guide demands ("log the state, not the symptom").

#include <filesystem>
#include <string>
#include <vector>

#include "common.h"
#include "engine/cmdline.h"

namespace os::fs {

// NOTE: this used to be named SearchPath, which collides with the Win32 macro
// `#define SearchPath SearchPathW` from windows.h - the first build failed with a
// bewildering "std::vector: 'SearchPathW' is not a valid template type". Renamed.
struct DirEntry {
    std::filesystem::path dir;
    std::string tag;  // "UserData", "BaseData", "Locale/en-US", "Config", "Data", "cwd"
};

void init(const Args& args, const std::filesystem::path& exe_dir);

const std::vector<DirEntry>& search_paths();

// Resolve a game-relative path ("Data/Package/ep1.package.big") against the search list.
// Returns empty on miss and logs the miss once per site (consolidated).
std::filesystem::path resolve(const std::string& rel);

bool exists(const std::string& rel);

// The writable area (UserData), created on demand like the original does.
std::filesystem::path user_data_dir();
std::filesystem::path log_dir();

std::string dump_search_list();

}  // namespace os::fs
