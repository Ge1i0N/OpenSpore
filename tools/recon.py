"""Full static recon report for refs/bins/ReleaseGA/SporeApp.exe.

Writes analysis/recon.md (human readable, committed as durable evidence) covering:
  * PE header + section table with per-section entropy
  * the CodeView (RSDS) record -> which PDB this exe expects, and its GUID/age
  * Authenticode certificate subject/issuer (proves whether the exe is stock EA or repacked)
  * version resource fields
  * imported DLLs and, for the interesting ones, the imported function names

Everything here is read straight out of the shipped binary. Authority level 1
(see PROMPT.md / ai-brief-and-commands.md): no claim in the report is inferred.

Usage: python tools/recon.py
"""
from __future__ import annotations

import hashlib
import math
import re
import struct
import sys
from collections import Counter
from pathlib import Path

_ROOT = Path(__file__).resolve().parent.parent
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

import pefile

from tools.pe import DEFAULT_EXE, PE, ROOT

OUT = ROOT / "analysis" / "recon.md"

INTERESTING_DLLS = ("d3d9.dll", "d3dx9_27.dll", "dsound.dll", "dinput8.dll", "winmm.dll", "dbghelp.dll", "steam_api.dll")


def entropy(data: bytes) -> float:
    if not data:
        return 0.0
    c = Counter(data)
    n = len(data)
    return -sum((v / n) * math.log2(v / n) for v in c.values())


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


SEC_FLAGS = [
    (0x00000020, "CNT_CODE"),
    (0x00000040, "CNT_INIT_DATA"),
    (0x00000080, "CNT_UNINIT_DATA"),
    (0x02000000, "MEM_PRIVATE"),
    (0x04000000, "MEM_REMOVE"),
    (0x08000000, "MEM_SECURE"),
    (0x10000000, "MEM_NOT_CACHED"),
    (0x20000000, "MEM_WRITE"),
    (0x40000000, "MEM_READ"),
    (0x80000000, "MEM_EXECUTE"),
    (0x00000100, "ALIGN_1"),
    (0x00008000, "MEM_TSD"),
    (0x00010000, "MEM_PAGED"),
    (0x00400000, "MEM_NOT_PAGED"),
    (0x00800000, "LNK_INFO"),
    (0x01000000, "LNK_NV_COFF"),
]


def _sec_flags(chars: int) -> str:
    names = [n for bit, n in SEC_FLAGS if chars & bit]
    return "|".join(names) if names else "-"


def codeview(pe: PE) -> list[str]:
    out = []
    # fast_load skips the debug directory, so re-open the file fully parsed.
    pep = pefile.PE(str(pe.path))
    if not hasattr(pep, "DIRECTORY_ENTRY_DEBUG"):
        return ["(no debug directory)"]
    for e in pep.DIRECTORY_ENTRY_DEBUG:
        d = e.struct
        out.append(
            f"type={d.Type} stamp={d.TimeDateStamp} addrOfRawData=0x{d.AddressOfRawData:08x} "
            f"ptrToRawData=0x{d.PointerToRawData:08x} sizeOfData={d.SizeOfData}"
        )
        if d.PointerToRawData and d.SizeOfData:
            raw = pep.__data__[d.PointerToRawData : d.PointerToRawData + d.SizeOfData]
            sig = raw[:4]
            out.append(f"  signature={sig.decode('latin-1') if sig in (b'RSDS', b'NB10', b'ELFM') else sig.hex()}")
            if sig == b"RSDS":
                # IMAGE_CVS_SIGNATURE_RSDS: Guid(16, little-endian mixed) + Age(4) + FileName(ANSI)
                import uuid

                u = uuid.UUID(bytes_le=raw[4:20])
                age = struct.unpack("<I", raw[20:24])[0]
                pdb = raw[24:].split(b"\x00")[0].decode("latin-1")
                out.append(f"  guid={u} age={age}")
                out.append(f"  pdb_path={pdb}")
            elif sig == b"NB10":
                out.append(f"  raw={raw.hex()}")
    return out


def cert(path: Path) -> list[str]:
    out = []
    pep = pefile.PE(str(path), fast_load=True)
    dd = pep.OPTIONAL_HEADER.DATA_DIRECTORY[4]
    if not dd.VirtualAddress or not dd.Size:
        return ["(no security directory / not signed)"]
    # The certificate is stored at a FILE offset, despite living in the "RVA" field.
    blob = pep.__data__[dd.VirtualAddress : dd.VirtualAddress + dd.Size]
    if len(blob) < 8:
        return [f"(short cert blob: {len(blob)} bytes)"]
    # WIN_CERTIFICATE = { DWORD dwLength; WORD wRevision; WORD wCertificateType; BYTE bCertificate[] }
    dw_len, w_rev, w_type = struct.unpack("<IHH", blob[:8])
    out.append(f"win_certificate: dwLength={dw_len} wRevision=0x{w_rev:04x} wType=0x{w_type:04x} dir_size={dd.Size}")
    body = blob[8:dw_len] if 8 <= dw_len <= len(blob) else b""
    out.append(f"  certificate body: {len(body)} bytes, sha256={hashlib.sha256(body).hexdigest() if body else '-'}")
    try:
        from cryptography.hazmat.primitives.serialization import pkcs7

        # cryptography 44 returns a plain list of certificates (older versions returned
        # (certificates, crls)); handle both without guessing.
        result = pkcs7.load_der_pkcs7_certificates(body)
        if isinstance(result, list):
            certs = result
        else:
            certs = list(result[0])
        for c in certs:
            out.append(f"  subject    : {c.subject.rfc4514_string()}")
            out.append(f"  issuer     : {c.issuer.rfc4514_string()}")
            out.append(f"  serial     : {c.serial_number:X}")
            out.append(f"  validity   : {c.not_valid_before_utc} .. {c.not_valid_after_utc}")
    except Exception as exc:  # pragma: no cover - a reporting tool must never crash
        out.append(f"  (PKCS7 parse failed: {type(exc).__name__}: {exc})")
    return out


def _rsrc_find(pe: PE, type_id: int):
    """Yield (name_or_id, data_rva, size) for every leaf of a resource type."""
    import pefile

    pep = pefile.PE(str(pe.path))
    if not hasattr(pep, "DIRECTORY_ENTRY_RESOURCE"):
        return
    for entry in pep.DIRECTORY_ENTRY_RESOURCE.entries:
        if entry.id != type_id:
            continue
        for name_entry in entry.directory.entries:
            uname = None if name_entry.name is None else name_entry.name.decode("latin-1", "replace")
            for lang in name_entry.directory.entries:
                d = lang.data
                yield uname, name_entry.id, pe.base + d.struct.OffsetToData, d.struct.Size


VS_VERSIONINFO_FIXED_SIZE = 0x34


def _vs_children(buf: bytes, pos: int, end: int):
    """Walk VS_VERSIONINFO child blocks starting at `pos`."""
    while pos + 6 <= end:
        wlen, wval, wtype = struct.unpack("<HHH", buf[pos : pos + 6])
        if wlen == 0 or pos + wlen > end:
            return
        # key: null-terminated UTF-16 starting at pos+6
        k = pos + 6
        chars = []
        while k + 1 < end:
            u = buf[k] | (buf[k + 1] << 8)
            if u == 0:
                break
            chars.append(chr(u) if 32 <= u <= 0x7E else "?")
            k += 2
        key = "".join(chars)
        v = (k + 2 + 3) & ~3  # value starts here, DWORD-aligned
        # MSDN: for text blocks (wType==1) wValueLength counts CHARACTERS, not bytes.
        vlen = wval * 2 if wtype == 1 else wval
        yield key, wtype, buf[v : v + vlen], pos, pos + wlen
        yield from _vs_children(buf, (v + vlen + 3) & ~3, pos + wlen)
        pos += wlen


def _vs_fixed(pe: PE, va: int, size: int) -> bytes:
    """Return the 52-byte VS_FIXEDFILEINFO of an RT_VERSION block, or b'' if malformed."""
    buf = pe.read(va, size)
    (_wlen, wval, _wtype) = struct.unpack("<HHH", buf[0:6])
    off = (6 + (len("VS_VERSION_INFO") + 1) * 2 + 3) & ~3
    if wval < VS_VERSIONINFO_FIXED_SIZE or off + VS_VERSIONINFO_FIXED_SIZE > len(buf):
        return b""
    return buf[off : off + VS_VERSIONINFO_FIXED_SIZE]


def _vs_strings(pe: PE, va: int, size: int) -> list[str]:
    out = []
    buf = pe.read(va, size)
    (wlen, wval, wtype) = struct.unpack("<HHH", buf[0:6])
    off = 6 + (len("VS_VERSION_INFO") + 1) * 2
    off = (off + 3) & ~3
    if wval < VS_VERSIONINFO_FIXED_SIZE:
        return [f"  (wValueLength={wval} too small for VS_FIXEDFILEINFO)"]
    (
        sig,
        str_ver,
        file_ms,
        file_ls,
        prod_ms,
        prod_ls,
        flags_mask,
        flags,
        file_os,
        file_type,
        file_subtype,
        date_ms,
        date_ls,
    ) = struct.unpack("<13I", buf[off : off + VS_VERSIONINFO_FIXED_SIZE])
    if sig != 0xFEEF04BD:
        return [f"  (bad VS_FIXEDFILEINFO signature 0x{sig:08x} at 0x{va+off:08x})"]
    out.append("  product version = %d.%d.%d.%d" % (prod_ms >> 16, prod_ms & 0xFFFF, prod_ls >> 16, prod_ls & 0xFFFF))
    out.append("  file    version = %d.%d.%d.%d" % (file_ms >> 16, file_ms & 0xFFFF, file_ls >> 16, file_ls & 0xFFFF))
    out.append(
        "  struct_ver=%#x flags=%#06x flags_mask=%#06x file_os=%d file_type=%d file_subtype=%d"
        % (str_ver, flags, flags_mask, file_os, file_type, file_subtype)
    )
    child = (off + wval + 3) & ~3
    for key, ktype, val, _lo, _hi in _vs_children(buf, child, len(buf)):
        if key in ("VarFileInfo", "StringFileInfo"):
            continue  # container blocks, no useful value bytes
        if key == "Translation":
            pairs = struct.unpack("<%dI" % (len(val) // 4), val[: len(val) & ~3]) if len(val) >= 4 else ()
            out.append(
                "  Translation: " + ", ".join("%04x/%04x" % (pairs[i], pairs[i + 1]) for i in range(0, len(pairs) - 1, 2))
            )
        elif re.fullmatch(r"[0-9a-fA-F]{8}", key):
            out.append(f"  StringTable {key}")
        else:
            enc = "utf-16-le" if ktype == 1 else "latin-1"
            out.append(f"    {key} = {val.decode(enc, 'replace').rstrip(chr(0))!r}")
    return out


def version_resource(pe: PE) -> list[str]:
    """Parse VS_FIXEDFILEINFO + StringFileInfo straight out of .rsrc.

    pefile's VERSION_INFO helper came back empty on this binary, so this walks the
    resource directory itself (RT_VERSION = 16). Authority: the bytes of .rsrc.

    The binary carries one RT_VERSION block per localized language. The fixed info is
    identical in all of them (verified), so the report prints it once and then only the
    en-US (0409) string table plus a count of the remaining languages.
    """
    blocks = list(_rsrc_find(pe, 16))
    if not blocks:
        return ["(no RT_VERSION)"]
    out: list[str] = []
    out.append(f"RT_VERSION blocks: {len(blocks)} (one per localized language)")
    # Fixed info from the first block; assert the rest agree.
    first = blocks[0]
    out.append(f"from va=0x{first[2]:08x} size={first[3]}:")
    out.extend(_vs_strings(pe, first[2], first[3])[:3])
    diffs = [b for b in blocks[1:] if _vs_fixed(pe, b[2], b[3]) != _vs_fixed(pe, first[2], first[3])]
    out.append(f"  VS_FIXEDFILEINFO blocks differing from the first: {len(diffs)} (of {len(blocks) - 1} others)")
    # Prefer the en-US table for the readable strings.
    chosen = None
    for uname, rid, va, size in blocks:
        txt = "".join(_vs_strings(pe, va, size))
        if "0409" in txt or "040904b0" in txt:
            chosen = (va, size)
            break
    if chosen is None:
        chosen = (first[2], first[3])
    out.append(f"string table from va=0x{chosen[0]:08x}:")
    for ln in _vs_strings(pe, chosen[0], chosen[1]):
        out.append("  " + ln.strip() if not ln.startswith(" ") else ln)
    return out


def manifest(pe: PE) -> list[str]:
    out = []
    for uname, rid, va, size in _rsrc_find(pe, 24):  # RT_MANIFEST
        raw = pe.read(va, size)
        out.append(f"RT_MANIFEST name={uname} id={rid} size={size}")
        out.append(raw.decode("utf-8", "replace"))
    return out


def imports(pe: PE) -> list[str]:
    out = []
    pep = pe.pe
    pep.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
    for e in getattr(pep, "DIRECTORY_ENTRY_IMPORT", []):
        dll = e.imported_as_dll.decode("latin-1") if hasattr(e, "imported_as_dll") else e.dll.decode("latin-1")
        names = []
        for imp in e.imports:
            if imp.name:
                names.append(imp.name.decode("latin-1"))
            elif imp.ordinal is not None:
                names.append(f"#{imp.ordinal}")
        flag = " *" if dll.lower() in INTERESTING_DLLS else ""
        out.append(f"{dll}{flag}: {len(names)} -> {', '.join(sorted(names)[:400])}")
    return out


def main() -> int:
    pe = PE()
    path = pe.path
    lines: list[str] = []
    lines.append("# Static recon: SporeApp.exe (Galactic Adventures, retail)")
    lines.append("")
    lines.append(f"Generated by `tools/recon.py`. Every value below is read out of the shipped binary.")
    lines.append("")
    lines.append(f"- path   : `{path.relative_to(ROOT)}`")
    lines.append(f"- size   : {path.stat().st_size:,} bytes")
    lines.append(f"- sha256 : `{sha256(path)}`")
    lines.append("")
    lines.append("## Header")
    lines.append("")
    lines.append("```")
    lines.append(pe.summary())
    lines.append("```")
    lines.append("")
    lines.append("## Section entropy + characteristics")
    lines.append("")
    lines.append("| section | va | vsize | raw | chars | flags | entropy | verdict |")
    lines.append("|---|---|---|---|---|---|---|---|")
    for s in sorted(pe.sections.values(), key=lambda x: x["rva"]):
        data = pe.section_bytes(s["name"])
        e = entropy(data)
        verdict = "**entropy 8.0 -> encrypted**" if e > 7.99 else "plaintext"
        lines.append(
            f"| `{s['name']}` | 0x{s['va']:08x} | 0x{s['vsize']:x} | 0x{s['raw']:x} | "
            f"0x{s['chars']:08x} | {_sec_flags(s['chars'])} | {e:.3f} | {verdict} |"
        )
    lines.append("")
    lines.append("Entry point: **0x%08x**, which lies in `%s`." % (
        pe.base + pe.pe.OPTIONAL_HEADER.AddressOfEntryPoint,
        next((s["name"] for s in pe.sections.values()
              if s["va"] <= pe.base + pe.pe.OPTIONAL_HEADER.AddressOfEntryPoint < s["end_va"]), "?"),
    ))
    lines.append("")
    lines.append("## CodeView / PDB binding")
    lines.append("")
    lines.append("```")
    lines.extend(codeview(pe) or ["(none)"])
    lines.append("```")
    lines.append("")
    lines.append("## Authenticode certificate")
    lines.append("")
    lines.append("```")
    lines.extend(cert(path) or ["(none)"])
    lines.append("```")
    lines.append("")
    lines.append("## Version resource (.rsrc RT_VERSION)")
    lines.append("")
    lines.append("```")
    lines.extend(version_resource(pe) or ["(none)"])
    lines.append("```")
    lines.append("")
    lines.append("## Manifest (.rsrc RT_MANIFEST)")
    lines.append("")
    lines.append("```")
    lines.extend(manifest(pe) or ["(none)"])
    lines.append("```")
    lines.append("")
    lines.append("## Imports")
    lines.append("")
    lines.append("`*` marks the DLLs that matter for the reimplementation (renderer/audio/input/steam).")
    lines.append("")
    for ln in imports(pe):
        lines.append(f"- {ln}")
    lines.append("")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)} ({len(lines)} lines)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
