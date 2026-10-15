#include "engine/starsdb.h"
#include <bit>
#include <utility>

namespace os::starsdb {
namespace {
u32 be32(const std::vector<u8>& bytes, size_t at) {
    return (u32(bytes[at])<<24) | (u32(bytes[at+1])<<16) | (u32(bytes[at+2])<<8) | bytes[at+3];
}
// Call only for a span already bounded by parse_frame. Count is BE, units are native UTF16LE.
bool decode_wide(const std::vector<u8>& bytes, const serialized::FieldSpan& field,
                 std::u16string& text, std::string& error) {
    if (field.size < 4) { error="truncated UTF16 string length"; return false; }
    const size_t count=be32(bytes,field.offset);
    if (count > 1048576 || count != (field.size-4)/2 || (field.size-4)%2) {
        error="invalid UTF16 string length or host string limit"; return false;
    }
    text.reserve(count);
    for (size_t n=0; n<count; ++n) {
        const size_t at=field.offset+4+2*n;
        text.push_back((char16_t)(u32(bytes[at]) | (u32(bytes[at+1])<<8)));
    }
    return true;
}
bool decode_header(const std::vector<u8>& bytes, const serialized::Frame& frame, Header& result, std::string& error) {
    for (const auto& field : frame.fields) {
        if (!field.size) continue; // GA skips zero-size fields; keep optional/default absent.
        switch (field.id) {
        case 0x03f140b6: case 0x03f140b7: {
            if (field.size != 4) { error="version field must contain exactly one BE word"; return false; }
            auto& version = field.id==0x03f140b6 ? result.saved_game_version_on_save : result.saved_game_version_on_generate;
            version = be32(bytes,field.offset);
            break;
        }
        case 0x03f140b8: case 0x03f140b9: {
            std::u16string text;
            if (!decode_wide(bytes,field,text,error)) return false;
            auto& build = field.id==0x03f140b8 ? result.build_version_on_save : result.build_version_on_generate;
            build=std::move(text);
            break;
        }
        case 0x03f140ba: case 0x03f140bb: {
            if (field.size != 36) { error="timestamp field must contain nine BE words"; return false; }
            std::array<u32,9> words{};
            for (size_t i=0; i<words.size(); ++i) words[i]=be32(bytes,field.offset+4*i);
            auto& stamp = field.id==0x03f140ba ? result.timestamp_on_save : result.timestamp_on_generate;
            stamp=words;
            break;
        }
        default: break; // unknown payload remains a bounded span, never guessed into a type.
        }
    }
    return true;
}
}

bool inspect_prefix(const std::vector<u8>& bytes, Prefix& out, std::string& error) {
    Prefix result;
    if (!serialized::parse_frame(bytes,0,serialized::kObjectTag,result.metadata,error) ||
        !decode_header(bytes,result.metadata,result.header,error) ||
        !serialized::parse_frame(bytes,result.metadata.end,serialized::kObjectTag,result.manager,error)) return false;
    result.consumed=result.manager.end;
    result.remaining=bytes.size()-result.consumed;
    out=std::move(result);
    error.clear();
    return true;
}

bool inspect_star_record(const std::vector<u8>& bytes, size_t offset, StarRecord& out, std::string& error) {
    StarRecord result;
    if (!serialized::parse_frame(bytes,offset,serialized::kObjectTag,result.frame,error)) return false;
    const auto scalar=[&](const serialized::FieldSpan& field,std::optional<u32>& value) {
        if (field.size!=4) { error="StarRecord scalar must contain exactly one BE word"; return false; }
        value=be32(bytes,field.offset);
        return true;
    };
    for (const auto& field : result.frame.fields) {
        if (!field.size) continue;
        bool ok=true;
        switch (field.id) {
        case 0x03d58bf2: ok=scalar(field,result.key); break;
        case 0x03e95e04: ok=scalar(field,result.type); break;
        case 0x03d58bf6: ok=scalar(field,result.tech_level); break;
        case 0x03d58bf3: ok=scalar(field,result.flags); break;
        case 0x03d58bf7: ok=scalar(field,result.starter_world_id); break;
        case 0x03f1389b: ok=scalar(field,result.empire_id); break;
        case 0x03f1389a: ok=scalar(field,result.last_observed_time); break;
        case 0x03f1389d: ok=scalar(field,result.saved_game_version_major); break;
        case 0x03f1389e: ok=scalar(field,result.saved_game_version_minor); break;
        case 0x03d58bf1: {
            if (field.size!=12) { error="StarRecord position must contain three BE float words"; return false; }
            std::array<float,3> position{};
            for (size_t i=0; i<3; ++i) position[i]=std::bit_cast<float>(be32(bytes,field.offset+4*i));
            result.position=position;
            break;
        }
        case 0x03d58bf4: {
            std::u16string text;
            if (!decode_wide(bytes,field,text,error)) return false;
            result.name=std::move(text);
            break;
        }
        case 0x03d58bf5: {
            if (field.size!=12) { error="StarRecord species key must contain three BE words"; return false; }
            // GA 0xAE2E50 reads into +8, +4, +0 in that order.
            result.citizen_species_key={{be32(bytes,field.offset+8),be32(bytes,field.offset+4),be32(bytes,field.offset)}};
            break;
        }
        case 0x03f1389c: {
            if (field.size!=36) { error="StarRecord timestamp must contain nine BE words"; return false; }
            std::array<u32,9> stamp{};
            for (size_t i=0; i<9; ++i) stamp[i]=be32(bytes,field.offset+4*i);
            result.saved_game_timestamp=stamp;
            break;
        }
        case 0x051a1188:
            if (field.size!=1) { error="StarRecord planet count must contain one byte"; return false; }
            result.planet_count=bytes[field.offset];
            break;
        default: break;
        }
        if (!ok) return false;
    }
    out=std::move(result);
    error.clear();
    return true;
}

const char* manager_field_name(u32 id) {
    // GA static constructor 0x1326120: names label payloads, NOT implemented object decoders.
    switch (id) {
    case 0x03fabad0: return "mStarRecordGrid";
    case 0x03ea8676: return "mStarterStarRecords";
    case 0x03ea8679: return "mSavedGameStarRecords";
    case 0x055ceb62: return "mBlackHoles";
    case 0x04274872: return "mEmpireHomeStarRecords";
    case 0x0563c450: return "mPossibleStartLocations";
    case 0x03f5bbc2: return "mSol";
    case 0x044ee63e: return "mpGlobalCLGItems";
    case 0x044ee640: return "mNextPoliticalID";
    case 0x044ee641: return "mGrobID";
    case 0x052c47aa: return "mEmpires";
    case 0x05528d40: return "mTradeRouteManager";
    case 0x0579f61c: return "sGameTimeManagerContainer";
    case 0x056e616c: return "sRelationshipManagerContainer";
    case 0x05d55277: return "sNameCountMap";
    case 0x05d55278: return "mTransactionLog";
    case 0x0563c451: return "mAvailableStarterWorlds";
    case 0x06b94eb9: return "mEmpireNamesInUse";
    case 0x0799479d: return "mAdventureIDs";
    default: return nullptr;
    }
}
}  // namespace os::starsdb
