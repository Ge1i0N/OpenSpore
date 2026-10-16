#pragma once
#include "engine/gge_catalog.h"

namespace os::gge {
// Caller-selected, line-oriented UTF-8 capture. No directory/profile search, save I/O or defaults.
// Successful load publishes a complete CatalogInput atomically; malformed/Unknown/incomplete input
// leaves the output unchanged. Names are hex-encoded UTF-8 so tabs/newlines cannot alter framing.
bool load_catalog_snapshot(const std::string& path,CatalogInput& out,std::string& error);
} // namespace os::gge
