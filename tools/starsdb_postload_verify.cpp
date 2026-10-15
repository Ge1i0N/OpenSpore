#include "engine/starsdb.h"
#include <iostream>
using namespace os;
using namespace os::starsdb;
int main() {
    size_t checks=0;
    const auto check=[&](bool ok,const char* why) { ++checks; if (!ok) std::cerr<<why<<'\n'; return ok; };
    StarRecord star;
    auto plan=observe_post_load(star,std::nullopt);
    if (!check(plan.name_blocked && plan.name_calls.empty() && plan.species_profile==ProfileObservation::Unknown,
               "omitted serialized name remains unknown")) return 1;
    for (const auto& name : std::vector<std::u16string>{u"Sol",u"bad_data",u"BAD_DATA!",u"\u0418",std::u16string{0,u'X'}}) {
        star.name=name; plan=observe_post_load(star,true);
        if (!check(!plan.name_blocked && plan.name_calls.empty() && star.name==name &&
            plan.species_profile==ProfileObservation::Found && plan.runtime_word_80_reset_required &&
            !plan.mark_profile_missing_required && plan.side_effects_suppressed,"ordinary name preserved; no canonical comparison")) return 1;
    }
    star.name=u"BAD_DATA"; plan=observe_post_load(star,false);
    if (!check(plan.name_blocked && plan.name_calls==std::vector<NameCall>{NameCall::GenerateBadData} &&
               plan.mark_profile_missing_required && plan.species_profile==ProfileObservation::Missing,"BAD_DATA EQUALITY requests generation")) return 1;
    NameGenerationObservations generated; generated.after_bad_data=u"Sol";
    plan=observe_post_load(star,true,generated);
    if (!check(!plan.name_blocked && plan.name_calls==std::vector<NameCall>{NameCall::GenerateBadData,NameCall::ApplyGeneratedName} &&
               *star.name==u"BAD_DATA","generated name requires setter but never mutates record")) return 1;
    generated.after_bad_data=u""; plan=observe_post_load(star,true,generated);
    if (!check(plan.name_blocked && plan.name_calls.back()==NameCall::GenerateRetryEmpty,"empty generated retry blocks without observation")) return 1;
    generated.after_retry=u""; plan=observe_post_load(star,true,generated);
    if (!check(!plan.name_blocked && plan.name_calls.back()==NameCall::ApplyGeneratedName && plan.name_calls.size()==3,
               "retry even empty has no unbounded loop")) return 1;
    star.name=u""; generated={}; plan=observe_post_load(star,std::nullopt,generated);
    if (!check(plan.name_blocked && plan.name_calls==std::vector<NameCall>{NameCall::GenerateInitialEmpty},"observed empty requests generator")) return 1;
    generated.after_initial=u""; plan=observe_post_load(star,std::nullopt,generated);
    if (!check(!plan.name_blocked && plan.name_calls.size()==1,"empty initial result is not BAD_DATA")) return 1;
    generated.after_initial=u"BAD_DATA"; generated.after_bad_data=u""; generated.after_retry=u"Nova";
    plan=observe_post_load(star,std::nullopt,generated);
    if (!check(!plan.name_blocked && plan.name_calls==std::vector<NameCall>{NameCall::GenerateInitialEmpty,NameCall::GenerateBadData,
        NameCall::GenerateRetryEmpty,NameCall::ApplyGeneratedName} && star.name->empty(),"ordered maximum four calls no mutation")) return 1;
    star.name=std::u16string{u'B',u'A',u'D',u'_',u'D',u'A',u'T',u'A',0,u'X'};
    plan=observe_post_load(star,std::nullopt);
    if (!check(plan.name_blocked && plan.name_calls==std::vector<NameCall>{NameCall::GenerateBadData},"native comparison terminates at zero")) return 1;
    generated={}; generated.after_bad_data=u"BAD_DATA"; plan=observe_post_load(star,std::nullopt,generated);
    if (!check(!plan.name_blocked && plan.name_calls.size()==2,"BAD_DATA regeneration not recursively repeated")) return 1;
    std::cout<<checks<<" corrected post-load name/profile decision checks PASS; controlled results, no services executed\n";
    return 0;
}
