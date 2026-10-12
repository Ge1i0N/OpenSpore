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
// STILL NOT REVERSED (do not pretend otherwise):
//   * The RefPack bit-level decoder. Entry points found and confirmed in the oracle:
//     0x0092ccdc (header: swaps the magic word, tests bit 0x8000 to choose a 3- or 4-byte
//     big-endian length, then calls 0x0092c5b0), 0x0092cd40 (detector: bswap(word)&0x1FFF
//     == 0x10FB). tools/refpack.py is UNVERIFIED and still faults on a real stream - its
//     command table was transcribed from a third-party doc instead of that disassembly.
//   * The `group`/`unknown` semantics, and why K=4 carries an extra word.
//   * Type ids are NOT FNV of their name (see IDEAS.md): reg_type.txt pairs such as
//     rw4=0x2f4e681b match no FNV variant (0/6 tested), so type ids are assigned, not hashed.
#include <cstdio>
#include <string>
#include <vector>

#include "common.h"

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

    u32 type() const { return raw[i_type]; }
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
    // Opens and validates the header. On failure, ok()==false and why() says which of the
    // validator's clauses rejected it - never a dialog, never a throw (PROMPT.md).
    bool open(const std::string& path);
    void close();

    bool ok() const { return m_ok; }
    const std::string& why() const { return m_why; }
    const Header& header() const { return m_hdr; }

    // True when this file's index matched one of the three observed kinds and was parsed.
    bool index_parsed() const { return m_kind_known; }
    u32 kind() const { return m_hdr.kind; }
    const SharedFields& shared() const { return m_shared; }

    const std::vector<Record>& records() const { return m_records; }

    // Read a resource's PACKED bytes. Returns false if the record is out of range or the
    // read fails; never asserts. RefPack payloads come back still compressed until
    // tools/refpack.py is verified against the original's decoder (see header note).
    bool read_resource(u32 record_index, std::vector<u8>& out_bytes);

    std::string describe() const;  // one-line summary for the log and the loader screen

private:
    Header m_hdr{};
    bool m_ok = false;
    bool m_kind_known = false;
    std::string m_why;
    std::string m_path;
    SharedFields m_shared{};
    std::vector<Record> m_records;
    FILE* m_file = nullptr;
};

// Enumerate every package under a directory (the loader's "what is actually here" view).
std::vector<std::string> find_packages(const std::string& dir, int max_depth = 4);

}  // namespace os::pkg
