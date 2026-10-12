#pragma once
// Common types and the original's own name-hashing primitive.
//
// FNV-1a 32-bit is not a stylistic choice: the disassembled AppStateManager registers
// states by FNV hash of their name. Evidence, refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe:
//
//   007e40f2:  push   0x811c9dc5                  ; FNV-1a 32-bit offset basis
//   007e40f7:  push   eax                         ; name pointer
//   007e40f8:  call   0x932e50                    ; -> hash
//   007e4103:  mov    DWORD PTR [ecx+0x28],eax    ; stored as the state's ID
//   007e4132:  cmp    ecx,DWORD PTR [eax+edx*4]   ; duplicate-ID probe
//   007e413b:  push   0x1413600                   ; "A state with this ID already exists"
//
// So a state's identity in this engine is a 32-bit FNV hash, and collisions are rejected
// at registration. We reproduce that exactly, including the rejection.
#include <cstdint>
#include <cstddef>

namespace os {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using f32 = float;

constexpr u32 FNV_OFFSET_BASIS = 0x811c9dc5u;
constexpr u32 FNV_PRIME        = 0x01000193u;

inline constexpr u32 fnv1a(const char* s, u32 h = FNV_OFFSET_BASIS) {
    return (*s == '\0') ? h : fnv1a(s + 1, (h ^ static_cast<u32>(static_cast<u8>(*s))) * FNV_PRIME);
}

inline constexpr u32 fnv1a_upper(const char* s, u32 h = FNV_OFFSET_BASIS) {
    if (*s == '\0') return h;
    char c = *s;
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
    return fnv1a_upper(s + 1, (h ^ static_cast<u32>(static_cast<u8>(c))) * FNV_PRIME);
}

}  // namespace os
