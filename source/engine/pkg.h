#pragma once
// DBPF package reader. Built on fields proven by disassembly + the shipped files.
//
// WHAT IS REVERSED (authority #1, disasm of refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe):
//   * Header is 0x60 bytes. The loader at 0x008d8f70 reads exactly 0x60 and checks
//     `cmp eax,0x60`; the validator at 0x008d90b0 enforces, quoted:
//         cmp dword [ecx],0x46504244     ; +0x00 "DBPF"
//         cmp dword [ecx+0x4],0x3 / ja   ; +0x04 version <= 3
//         cmp dword [ecx+0x24],0x7ffffff ; +0x24 entry count < 0x07FFFFFF
//         mov edx,[ecx+0x40]; cmp edx,filesize; jae        ; +0x40 index_offset < filesize
//         mov esi,[ecx+0x2c]; add esi,edx; cmp esi,filesize; ja  ; +0x2c size + offset <= filesize
//         cmp dword [ecx+0x3c],0x0 / je  ; +0x3c non-zero
//     This reader REPLAYS those clauses in the same order with the same reasons instead of
//     inventing its own validation.
//   * The magic is an INLINE IMMEDIATE, not a string: "DBPF" occurs 0 times in .rdata/.text,
//     so a string xref cannot find the reader (tools/find_imm.py --ascii DBPF can).
//
// WHAT IS MEASURED ON ALL SHIPPED DATA (authority #2; tools/pkg_kinds.py):
//   * 57/57 files: index_offset + index_size == filesize exactly.
//   * 150,945 records across those files: zero violations of (offset < index_offset) and
//     (offset + packedSize <= index_offset).
//
// INDEX KINDS. The index's first dword (K) is a SHARED-FIELD BIT MASK, not a selector:
//   bit 0 set -> `type`    shared (one value in the prefix, absent from each record)
//   bit 1 set -> `group`   shared
//   bit 2 set -> `unknown` shared
// prefix = 4 + 4*popcount(K), stride = 32 - 4*popcount(K), because a full record is
//   type, group, unknown, instance, offset, packedSize, memSize, compression:u16, flags:u16 = 32 B
// Measured: K=4 -> (8,28)  K=6 -> (12,24)  K=7 -> (16,20).
// CORRECTION: an earlier version of this header claimed prefix = 4+K, stride = 32-K. That
// fits K=4 and K=7 but NOT K=6 (4+6=10, while the measured prefix is 12). The popcount form
// is the one that fits all three, and it is what the record layout implies.
//
// THE FORMER `sentinel` u32 IS REALLY TWO u16 FIELDS, and splitting it explains the two
// values it had. Verified by reading the first bytes at every record's offset across all
// 57 packages:
//   compression=0xFFFF (RefPack), flags=0x0001 : 98,407 records, payload starts 10 FB in
//                                                 100% of samples, packedSize < memSize
//   compression=0x0000 (raw),     flags=0x0001 : 52,538 records, payload starts 10 FB in
//                                                   0% of samples, packedSize == memSize
// So 0x80000000 on the size word is a validity/used bit, and compression is what toggles.
//
// REFPACK: SOLVED and verified (was "not reversed" in the previous revision of this note).
// `source/engine/refpack.cpp` decodes it, `Package::read_resource_decoded` wires it in, and
// tools/refpack.py --verify decodes 30,000/30,000 real records to exactly memSize (0 fails).
// Two instruction-level facts that cost real time and are easy to get wrong again:
//   * the copy loops test their counter BEFORE decrementing (0x92c660), so the byte count is
//     the register value PLUS ONE - using the raw value failed 250/250 sampled streams;
//   * the signature test masks (& 0x1FFF) == 0x10FB, so 0x10FB and 0x50FB are BOTH valid
//     (top bits are flags) - a strict data[0]==0x10 check rejected 50 of 20,000 records.
//
// STILL NOT REVERSED (do not pretend otherwise):
//   * The `group`/`unknown` semantics, and why K=4 carries an extra word.
//   * Type ids are NOT FNV of their name (see IDEAS.md): reg_type.txt pairs such as
//     rw4=0x2f4e681b match no FNV variant (0/6 tested), so type ids are assigned, not hashed.
#include <cstdio>
#include <string>
#include <cstring>
#include <vector>

#include "common.h"
#include "engine/typeids.inc"  // generated id->name table (tools/gen_typeids.py)

namespace os::pkg {

struct Header {
    u32 magic = 0;
    u32 version = 0;
    u32 count = 0;        // header +0x24
    u32 index_size = 0;   // header +0x2c
    u32 index_offset = 0; // header +0x40, or +0x28 when +0x40 == 0 (the loader's fallback)
    u32 field_3c = 0;     // header +0x3c, required non-zero
    u64 file_size = 0;
    u32 kind = 0;         // index dword[0]: a shared-field bit mask (4, 6, 7 observed)
    bool valid = false;
    std::string why;  // set when !valid, using the validator's own rejection reasons
};

struct Record {
    u32 raw[7];      // up to 7 words (K=4); unused slots are 0
    u32 nwords = 0;  // 5, 6 or 7
    // Field positions depend on the kind; the Package stores them so records stay flat.
    u32 i_type = 0, i_group = 0, i_unknown = 0, i_instance = 0, i_offset = 0, i_size = 0, i_usize = 0, i_flags = 0;
    bool has_group = false, has_unknown = false, has_instance = false;
    // When a field is SHARED (masked out of the record by the kind), its value lives once in
    // the index prefix, not in the record. `type()` must fall back to that value: for kind 7
    // there is no type word at all, word[0] is the instance, so reading raw[i_type] would have
    // returned the instance and printed a wrong type name.
    bool type_shared = false;
    u32 shared_type = 0;
    bool group_shared = false;
    u32 shared_group = 0;
    bool unknown_shared = false;
    u32 shared_unknown = 0;

    u32 type() const { return type_shared ? shared_type : raw[i_type]; }
    u32 group() const { return group_shared ? shared_group : (has_group ? raw[i_group] : 0); }
    u32 unknown() const { return unknown_shared ? shared_unknown : (has_unknown ? raw[i_unknown] : 0); }
    u32 instance() const { return has_instance ? raw[i_instance] : 0; }
    u32 offset() const { return raw[i_offset]; }
    u32 packed_size() const { return raw[i_size] & 0x7FFFFFFFu; }
    u32 size() const { return packed_size(); }  // kept: callers used size() before the split
    u32 stored_flag() const { return raw[i_size] >> 31; }
    u32 mem_size() const { return raw[i_usize]; }
    u32 usize() const { return mem_size(); }  // kept: callers used usize() before the split
    // The former single `sentinel` u32, split into the two u16 fields it really is.
    u16 compression() const { return (u16)(raw[i_flags] & 0xFFFFu); }  // 0xFFFF = RefPack, 0 = raw
    bool refpacked() const { return compression() == 0xFFFF; }
    u16 flags() const { return (u16)(raw[i_flags] >> 16); }  // 0x0001 on every record seen
};

// The shared prefix values, when the kind masks a field out of the records.
struct SharedFields {
    u32 type = 0, group = 0, unknown = 0;
    bool has_type = false, has_group = false, has_unknown = false;
};

class Package {
public:
    Package() = default;
    ~Package() { close(); }
    Package(const Package&) = delete;
    Package& operator=(const Package&) = delete;

    // Opens and validates the header. On failure, ok()==false and why() says which of the
    // validator's clauses rejected it - never a dialog, never a throw (PROMPT.md).
    bool open(const std::string& path);

    // Opens a DBPF that lives in MEMORY, i.e. a NESTED package. Spore ships those: the EP1 locale
    // text is inside DataEP1/Spore_EP1_Locale_0{1,2}.package as 21 resources of type `package`
    // (0x06EFC6AA), each of which is itself a whole DBPF - measured in tools/nested_locale.py, they
    // hold 1,364 `locale` resources, and the en-us one (instance 0x7080DB55 == fnv1_lower("en-us"),
    // so a nested package is named by the SAME hash rule as a locale table) carries 130 EP1 text
    // tables that our flat loader could not see. Same validation as the file path: the oracle's
    // validator does not care where the bytes came from.
    bool open_from_memory(const u8* data, size_t size, const std::string& label);

    void close();

    bool ok() const { return m_ok; }
    const std::string& why() const { return m_why; }
    // Where these bytes came from: a file path for open(), or the label for open_from_memory().
    // Needed because a definition can arrive from several containers at once, and knowing WHICH one
    // won is the difference between a measured precedence rule and an accident of traversal order.
    const std::string& path() const { return m_path; }
    const Header& header() const { return m_hdr; }

    // True when this file's index matched one of the three observed kinds and was parsed.
    bool index_parsed() const { return m_kind_known; }
    u32 kind() const { return m_hdr.kind; }
    const SharedFields& shared() const { return m_shared; }

    const std::vector<Record>& records() const { return m_records; }

    // Read a resource's PACKED bytes. Returns false if the record is out of range or the
    // read fails; never asserts.
    bool read_resource(u32 record_index, std::vector<u8>& out_bytes);

    // Read a resource and, when its index record says RefPack, decompress it (RefPack is
    // fully solved: tools/refpack.py --verify passes 30,000/30,000 real records). On success
    // out_bytes.size() == the record's memSize. Returns false and logs on any failure.
    bool read_resource_decoded(u32 record_index, std::vector<u8>& out_bytes);

    std::string describe() const;  // one-line summary for the log and the loader screen

private:
    // Read `n` bytes at `off` from whichever backing store this package has (file or memory).
    bool read_at(u64 off, void* dst, size_t n);
    // The header/index validation shared by both open paths, so a nested package is validated by
    // EXACTLY the same clauses as a file (the oracle's validator does not care where bytes came from).
    bool parse(u64 fsz);

    Header m_hdr{};
    bool m_ok = false;
    bool m_kind_known = false;
    std::string m_why;
    std::string m_path;
    SharedFields m_shared{};
    std::vector<Record> m_records;
    FILE* m_file = nullptr;
    std::vector<u8> m_mem;  // non-empty for a nested package opened from memory
};

// Enumerate every package under a directory (the loader's "what is actually here" view).
std::vector<std::string> find_packages(const std::string& dir, int max_depth = 4);

}  // namespace os::pkg
