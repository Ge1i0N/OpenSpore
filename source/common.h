#pragma once
// Common types and the original's own name-hashing primitive.
//
// FNV is not a stylistic choice, and getting its VARIANT wrong is a silent bug: the id is
// only ever compared to other ids, so a wrong variant still "works" inside our own build
// and only diverges from the game's own numbering.
//
// THE ORIGINAL IS FNV-1 (multiply-then-xor), NOT FNV-1a. Evidence, disasm of
// refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe, hash function at 0x00932e50:
//
//   932e8a: imul edi,edi,0x1000193     ; h *= prime   FIRST
//   932e90: xor  edi,eax               ; h ^= c       SECOND
//
// The first version of this header implemented FNV-1a (xor-then-multiply) with uppercase
// folding, which gave Loader=0x69cdcb22 where the original gives 0xd7d04c04.
//
// The function takes THREE arguments (cdecl: arg1=[esp+4] name, arg2=[esp+8] seed,
// arg3=[esp+0xc] mode), and mode selects the case folding, dispatched at 0x932e54:
//   mode 0 -> 0x932ed6 : raw bytes, no folding
//   mode 1 -> 0x932ea2 : MSVCR90!tolower  (IAT slot 0x013cc2ec, verified via pefile)
//   mode 2 -> 0x932e79 : MSVCR90!toupper  (IAT slot 0x013cc500, verified via pefile)
// The state-registration call site (0x007e40f2) pushes (name, 0x811c9dc5, 1), i.e.
// **tolower** with the standard FNV offset basis. So a game state id is FNV-1-lower.
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
constexpr u32 FNV_PRIME = 0x01000193u;

enum class FnvMode : u8 { Raw = 0, Lower = 1, Upper = 2 };  // matches the original's arg3

namespace detail {
constexpr u32 fold_char(char c, FnvMode m) {
    if (static_cast<u32>(c) > 0x7F) return static_cast<u32>(static_cast<u8>(c));
    if (m == FnvMode::Lower && c >= 'A' && c <= 'Z') return static_cast<u32>(c + 32);
    if (m == FnvMode::Upper && c >= 'a' && c <= 'z') return static_cast<u32>(c - 32);
    return static_cast<u32>(static_cast<u8>(c));
}
}  // namespace detail

// FNV-1: h = (h * prime) ^ c, exactly the instruction order seen at 0x932e8a.
constexpr u32 fnv1(const char* s, FnvMode m = FnvMode::Raw, u32 h = FNV_OFFSET_BASIS) {
    return (*s == '\0') ? h : fnv1(s + 1, m, ((h * FNV_PRIME) & 0xFFFFFFFFu) ^ detail::fold_char(*s, m));
}

// The id the game itself uses for a state / registered name: FNV-1 with tolower.
constexpr u32 fnv1_lower(const char* s, u32 h = FNV_OFFSET_BASIS) { return fnv1(s, FnvMode::Lower, h); }
constexpr u32 fnv1_upper(const char* s, u32 h = FNV_OFFSET_BASIS) { return fnv1(s, FnvMode::Upper, h); }

}  // namespace os
