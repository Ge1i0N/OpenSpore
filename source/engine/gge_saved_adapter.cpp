#include "engine/gge_saved_adapter.h"
#include <cmath>
#include <utility>

namespace os::gge {
bool adapt_saved_metadata(const starsdb::StarRecord& star,const std::vector<planet::ResourceRecord>& planets,
                          const SavedEntryObservations& observed,SavedGameMetadata& out,std::string& error) {
    if (!star.key || !star.position || !star.flags || (!(*star.flags&0x80u) && !star.tech_level)) {
        error="saved StarRecord key/position/flags/stage unavailable"; return false;
    }
    if ((observed.terrain!=TerrainQueryObservation::QueryFailed && observed.terrain!=TerrainQueryObservation::Loaded) || !observed.header_probe ||
        (observed.terrain==TerrainQueryObservation::Loaded && !observed.terrain_image)) {
        error="saved entry terrain/header observations unavailable"; return false;
    }
    for (float coordinate:*star.position) {
        if (!std::isfinite(coordinate)) { error="saved StarRecord position is nonfinite"; return false; }
    }
    planet::Selection selection;
    if (!planet::select_save_stem(star,planets,selection,error)) return false;
    if (!selection.source_index) {
        error="no selected planet name; original shared-string carry behavior not ported"; return false;
    }
    SavedGameMetadata result;
    result.star.star_id=*star.key;
    result.star.position={(*star.position)[0],(*star.position)[1],(*star.position)[2]};
    result.star.name=selection.save_stem;
    result.save_stem=std::move(selection.save_stem);
    result.source_flags=*star.flags;
    result.stage=(*star.flags&0x80u)?0:*star.tech_level;
    result.star.terrain_loaded=observed.terrain==TerrainQueryObservation::Loaded;
    result.star.terrain_image=result.star.terrain_loaded?*observed.terrain_image:0;
    result.probe_result=observed.header_probe->predicate;
    result.header=observed.header_probe->words;
    out=std::move(result); error.clear(); return true;
}
}  // namespace os::gge
