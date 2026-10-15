#include "engine/gge_catalog.h"
#include <iostream>
#include <limits>

using namespace os;
using namespace os::gge;

int main() {
    unsigned checks = 0;
    const auto require = [&](bool ok, const char* why) {
        ++checks;
        if (!ok) std::cerr << "GGE catalog: " << why << '\n';
        return ok;
    };
    CatalogInput input;
    std::vector<GameEntry> output(1);
    output[0].star_id = 0x12345678;
    std::string error;
    const auto reject = [&](const CatalogInput& bad) {
        output.assign(1, GameEntry{});
        output[0].star_id = 0x12345678;
        return !build_catalog(bad, output, error) && !error.empty() && output.size() == 1 &&
               output[0].star_id == 0x12345678;
    };
    if (!require(reject(input), "unknown input published an empty ready catalog")) return 1;
    input.saved_status = CatalogAvailability::Available;
    if (!require(reject(input), "unknown candidate list ignored")) return 1;
    input.candidate_status = CatalogAvailability::Available;
    if (!require(build_catalog(input, output, error) && output.empty() && error.empty(),
                 "explicit observed empty lists rejected")) return 1;
    SavedGameMetadata saved;
    saved.star = {42, {-500.f, 123.f, 700.f}, u"Saved \u0418\u0433\u0440\u0430", false, 999};
    saved.save_stem = u"save/path-not-read";
    saved.stage = 2;
    saved.probe_result = true;
    saved.header = {0x25, 3};
    input.saved.push_back(saved);
    saved.star.name = u"Same star ID is not a unique UI identity";
    saved.star.terrain_loaded = true;
    saved.star.terrain_image = 0;
    saved.source_flags = 0x80;
    saved.header.second = 4; // same first word, different second must not match
    input.saved.push_back(saved);
    input.candidates.push_back({42, {4,5,6}, u"Candidate", true, 0x99887766});
    input.candidates.push_back({0, {0,0,0}, u"", false, 0});
    if (!require(build_catalog(input,output,error) && output.size()==4 && error.empty(),"build/order size")) return 1;
    if (!require(output[0].saved && output[1].saved && !output[2].saved && !output[3].saved &&
                 output[0].source_index==0 && output[1].source_index==1 &&
                 output[2].source_index==0 && output[3].source_index==1,"saved/candidate order and indices")) return 1;
    if (!require(output[0].star_id==42 && output[1].star_id==42 && output[2].star_id==42 &&
                 output[0].name==input.saved[0].star.name && output[0].save_stem==saved.save_stem,
                 "duplicates/text altered")) return 1;
    if (!require(output[0].position.x==-500.f && output[0].position.z==700.f &&
                 output[2].position.x==4.f,"world position normalized or clamped")) return 1;
    if (!require(output[0].stage==2 && output[1].stage==0 && output[2].stage==5 &&
                 output[0].visible && output[2].visible,"stage flags/defaults")) return 1;
    if (!require(output[0].save_status==SaveStatus::HeaderMatches && output[1].save_status==SaveStatus::HeaderDiffers &&
                 output[2].save_status==SaveStatus::NotSaved,"two-word save-header status")) return 1;
    if (!require(output[0].image.instance==0x6837044 && output[0].image.group==0x2605ba3 &&
                 output[1].image.instance==0 && output[1].image.type==0x2f7d0004 &&
                 output[1].image.group==0x2e9a2ca && output[2].image.instance==0x99887766 &&
                 output[3].image.instance==0 && output[3].image.type==0,"resource fallback dimensions")) return 1;
    for (SaveHeaderWords header : {SaveHeaderWords{0,0}, {1,0}, {0,1}}) {
        input.saved[0].probe_result = false;
        input.saved[0].header = header;
        if (!require(build_catalog(input,output,error) && output[0].save_status ==
                     ((header.first || header.second) ? SaveStatus::ProbeFalseNonzeroHeader : SaveStatus::ProbeFalseZeroHeader),
                     "probe-false header dimensions")) return 1;
    }
    auto invalid = input;
    invalid.candidates[1].position.x=std::numeric_limits<float>::quiet_NaN();
    if (!require(reject(invalid),"NaN partial publication")) return 1;
    invalid=input; invalid.saved[1].star.position.z=std::numeric_limits<float>::infinity();
    if (!require(reject(invalid),"infinity partial publication")) return 1;
    invalid=input; invalid.saved[0].save_stem.assign(4097,u'a');
    if (!require(reject(invalid),"oversized save text accepted")) return 1;
    invalid=input; invalid.candidates[0].name.assign(4097,u'b');
    if (!require(reject(invalid),"oversized star name accepted")) return 1;
    invalid=input; invalid.candidates.resize(4096);
    if (!require(reject(invalid),"total count limit ignored")) return 1;
    input.saved.clear(); input.candidates.assign(4096,StarMetadata{});
    if (!require(build_catalog(input,output,error) && output.size()==4096,"host bound exact edge")) return 1;
    std::cout << checks << " GGE catalog projection/order/identity/atomic-rejection checks PASS\n";
    std::cout << "Controlled typed metadata only: not stars.db parsing, galaxy generation or a live catalog adapter\n";
}
