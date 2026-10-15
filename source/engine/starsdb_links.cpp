#include "engine/starsdb_links.h"
#include <utility>

namespace os::starsdb {
namespace {
class Cursor {
public:
    Cursor(const std::vector<u8>& bytes,size_t begin,size_t size) : bytes_(bytes), at_(begin), end_(begin) {
        valid_=begin<=bytes.size() && size<=bytes.size()-begin;
        if (valid_) end_=begin+size;
    }
    bool valid() const { return valid_; }
    size_t remaining() const { return end_-at_; }
    bool word(u32& value) {
        if (!valid_ || remaining()<4) return false;
        value=(u32(bytes_[at_])<<24) | (u32(bytes_[at_+1])<<16) | (u32(bytes_[at_+2])<<8) | bytes_[at_+3];
        at_+=4;
        return true;
    }
private:
    const std::vector<u8>& bytes_;
    size_t at_,end_;
    bool valid_=false;
};
bool list(Cursor& cursor,ReferenceList& out,size_t& budget,std::string& error) {
    u32 count=0;
    if (!cursor.word(count)) { error="truncated reference-list count"; return false; }
    if (count>budget) { error="reference count exceeds host aggregate limit"; return false; }
    if (count>cursor.remaining()/4) { error="reference count exceeds minimum payload size"; return false; }
    budget-=count;
    out.reserve(count);
    for (u32 i=0; i<count; ++i) {
        ObjectReference ref;
        if (!cursor.word(ref.token) || (ref.token && !cursor.word(ref.class_id))) {
            error="truncated serialized object reference"; return false;
        }
        out.push_back(ref);
    }
    return true;
}
}
bool read_reference_list(const std::vector<u8>& bytes,size_t offset,size_t size,
                         ReferenceList& out,std::string& error) {
    Cursor cursor(bytes,offset,size);
    if (!cursor.valid()) { error="reference-list span outside input"; return false; }
    ReferenceList result;
    size_t budget=kMaxReferences;
    if (!list(cursor,result,budget,error)) return false;
    if (cursor.remaining()) { error="reference list has unconsumed payload bytes"; return false; }
    out=std::move(result);
    error.clear();
    return true;
}
bool read_reference_grid(const std::vector<u8>& bytes,size_t offset,size_t size,
                         ReferenceGrid& out,std::string& error) {
    Cursor cursor(bytes,offset,size);
    if (!cursor.valid()) { error="reference-grid span outside input"; return false; }
    u32 cells=0;
    if (!cursor.word(cells)) { error="truncated reference-grid count"; return false; }
    if (cells>kMaxGridCells) { error="grid cell count exceeds host limit"; return false; }
    if (cells>cursor.remaining()/4) { error="grid count exceeds minimum cell payload size"; return false; }
    ReferenceGrid result;
    result.reserve(cells);
    size_t budget=kMaxReferences;
    for (u32 i=0; i<cells; ++i) {
        ReferenceList cell;
        if (!list(cursor,cell,budget,error)) return false;
        result.push_back(std::move(cell));
    }
    if (cursor.remaining()) { error="reference grid has unconsumed payload bytes"; return false; }
    out=std::move(result);
    error.clear();
    return true;
}
bool inspect_manager_links(const std::vector<u8>& bytes,size_t offset,ManagerLinks& out,std::string& error) {
    serialized::Frame frame;
    if (!serialized::parse_frame(bytes,offset,serialized::kObjectTag,frame,error)) return false;
    ManagerLinks result;
    for (const auto& field : frame.fields) {
        if (!field.size) continue;
        if (field.id==0x03fabad0) {
            ReferenceGrid grid;
            if (!read_reference_grid(bytes,field.offset,field.size,grid,error)) return false;
            result.grid=std::move(grid);
            continue;
        }
        std::optional<ReferenceList>* target=nullptr;
        switch (field.id) {
        case 0x03ea8676: target=&result.starters; break;
        case 0x03ea8679: target=&result.saved; break;
        case 0x055ceb62: target=&result.black_holes; break;
        case 0x0563c450: target=&result.possible_starts; break;
        default: break;
        }
        if (target) {
            ReferenceList refs;
            if (!read_reference_list(bytes,field.offset,field.size,refs,error)) return false;
            *target=std::move(refs);
        }
    }
    out=std::move(result);
    error.clear();
    return true;
}
}  // namespace os::starsdb
