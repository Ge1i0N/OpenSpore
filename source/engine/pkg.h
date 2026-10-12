#pragma once
// DBPF package reader. Built ONLY on fields proven by disassembly + the shipped files.
//
// What is confirmed (see VERIFY rows 18-21, MEMORY "DBPF package format"):
//   * Header is 0x60 bytes. The loader at 0x008d8f70 reads exactly 0x60 and checks
//     `cmp eax,0x60`; the validator at 0x008d90b0 enforces, quoted:
//         cmp dword [ecx],0x46504244     ; +0x00 "DBPF"
//         cmp dword [ecx+0x4],0x3 / ja   ; +0x04 version <= 3
//         cmp dword [ecx+0x24],0x7ffffff ; +0x24 entry count < 0x07FFFFFF
//         mov edx,[ecx+0x40]; cmp edx,filesize; jae      ; +0x40 index_offset < filesize
//         mov esi,[ecx+0x2c]; add esi,edx; cmp esi,filesize; ja  ; +0x2c size + +0x40 <= filesize
//         cmp dword [ecx+0x3c],0x0 / je  ; +0x3c non-zero
//   * Across all 57 shipped packages: index_offset + index_size == filesize exactly.
//   * For 31/57 files: index_size == 12 + 24*count, i.e. a 12-byte prefix then `count`
//     24-byte records.
//   * Record field [2] = file offset and [3] = size | 0x80000000, confirmed by 17,901
//     records with ZERO violations of (offset < index_offset) and (offset+size <= index_offset).
//
// What is NOT confirmed (kept as raw words, never interpreted):
//   record[0], record[1], record[4], record[5]. Observed distributions, from
//   tools/pkg_record_test.py over those 17,901 records:
//     [0]: 16 distinct values, one dominates (0x1A510BB, 98%) - a parent/collection id
//     [1]: varies per record - a candidate instanceId/hash
//     [3] high bit: set in 17901/17901 - so it is a constant flag on valid entries, not a
//         compression bit that toggles
//     [4]: >= masked [3] in 17901/17901 - consistent with "uncompressed size", unproven
//     [5]: exactly two values, 0x10000 (88%) and 0x1FFFF (12%)
//   The 25 files whose index_size != 12 + 24*count carry variable-width records (they hold
//   names) and are reported as UNSUPPORTED until the record parser is disassembled; that
//   list is printed by tools/pkg_record_test.py. Do not guess their layout.
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
    bool valid = false;
    std::string why;  // set when !valid, using the validator's own rejection reasons
};

struct Record {
    u32 raw[6];        // the six words, verbatim
    u32 offset() const { return raw[2]; }
    u32 size() const { return raw[3] & 0x7FFFFFFFu; }
    u32 stored_flag() const { return raw[3] >> 31; }
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

    // True when this file's index matches the proven fixed-width form (12 + 24*count).
    bool fixed_stride() const { return m_fixed; }

    const std::vector<Record>& records() const { return m_records; }

    // Read a resource's bytes. Returns false if the record is out of range or the read
    // fails; never asserts. This is the only data path the loader uses for now.
    bool read_resource(u32 record_index, std::vector<u8>& out_bytes);

    std::string describe() const;  // one-line summary for the log and the loader screen

private:
    Header m_hdr{};
    bool m_ok = false;
    bool m_fixed = false;
    std::string m_why;
    std::string m_path;
    std::vector<Record> m_records;
    FILE* m_file = nullptr;
};

// Enumerate every package under a directory (the loader's "what is actually here" view).
std::vector<std::string> find_packages(const std::string& dir, int max_depth = 4);

}  // namespace os::pkg
