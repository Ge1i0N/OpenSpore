#include "engine/starsdb.h"
#include <iostream>
using namespace os;
using namespace os::starsdb;
int main() {
    StarRecord record; record.name=std::u16string(u"Sol");
    auto matched=observe_post_load(record,true,std::u16string(u"Sol"));
    auto changed=observe_post_load(record,false,std::u16string(u"Sol System"));
    auto unknown=observe_post_load(record,std::nullopt,std::nullopt);
    const bool ok=matched.species_profile==Observation::Matched && matched.canonical_name==Observation::Matched &&
        matched.position_cache_reset && matched.side_effects_suppressed &&
        changed.species_profile==Observation::Missing && changed.canonical_name==Observation::Changed &&
        unknown.species_profile==Observation::Unknown && unknown.canonical_name==Observation::Changed;
    if (!ok) { std::cerr<<"post-load observation model failed\n"; return 1; }
    std::cout<<"post-load observation checks PASS; no fixup/name injection/side effect executed\n";
    return 0;
}
