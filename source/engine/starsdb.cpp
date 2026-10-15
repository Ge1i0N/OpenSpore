#include "engine/starsdb.h"
#include <utility>

namespace os::starsdb {
namespace {
u32 be32(const std::vector<u8>& bytes, size_t at) {
    return (u32(bytes[at])<<24) | (u32(bytes[at+1])<<16) | (u32(bytes[at+2])<<8) | bytes[at+3];
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
            if (field.size < 4) { error="truncated build string length"; return false; }
            const size_t count = be32(bytes,field.offset);
            // 0x693860: BE count, then 0x93A670(...,raw=1), hence native x86 UTF-16LE units.
            if (count > 1048576 || count != (field.size-4)/2 || (field.size-4)%2) {
                error="invalid build string length or host string limit"; return false;
            }
            std::u16string text;
            text.reserve(count);
            for (size_t n=0; n<count; ++n) {
                const size_t at=field.offset+4+2*n;
                text.push_back((char16_t)(u32(bytes[at]) | (u32(bytes[at+1])<<8)));
            }
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
