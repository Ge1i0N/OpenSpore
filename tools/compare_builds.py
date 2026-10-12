"""Decide which candidate binary is the *same build* as the target we must reimplement.

The field guide's rule: match by function identity, and state which version you verified
against. Since refs/bins/ReleaseGA/SporeApp.exe (the target) has an encrypted .text, the
only way to use a plaintext-.text sibling as the oracle is to *prove* it is the same link.

Proof ladder, strongest first:
  1. CodeView (RSDS) GUID+age equal  -> literally the same link output.
  2. .rdata + .data byte-hash equal  -> same compiler inputs for all read-only data;
                                        a different .text alone would mean a re-link with
                                        identical data, which is effectively impossible.
  3. import table + timestamp + version resource equal -> same build, weaker.

Usage: python tools/compare_builds.py
Writes: analysis/build_identity.md
"""
from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

_ROOT = Path(__file__).resolve().parent.parent
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

import pefile

TARGET = _ROOT / "refs" / "bins" / "ReleaseGA" / "SporeApp.exe"
CANDIDATES = [
    _ROOT / "refs" / "bins" / "FindTheUnencryptedDotText" / "Also1" / "SporeApp.exe",
    _ROOT / "refs" / "bins" / "FindTheUnencryptedDotText" / "Also1" / "SporeAppEP1.exe",
    _ROOT / "refs" / "bins" / "FindTheUnencryptedDotText" / "Also2" / "SporeApp.exe",
    _ROOT / "refs" / "bins" / "FindTheUnencryptedDotText" / "Also2" / "SporeAppEP1.exe",
    _ROOT / "refs" / "bins" / "FindTheUnencryptedDotText" / "Also2" / "SporeApp.wtf",
    _ROOT / "refs" / "bins" / "FindTheUnencryptedDotText" / "Also2" / "SporeAppEP1.wtf",
    _ROOT / "refs" / "bins" / "FindTheUnencryptedDotText" / "Also2" / "SporeApp.bgm",
    _ROOT / "refs" / "bins" / "FindTheUnencryptedDotText" / "Also2" / "SporeAppEP1.bgm",
    _ROOT / "refs" / "bins" / "FindTheUnencryptedDotText" / "Also3" / "SporeApp.exe",
    _ROOT / "refs" / "bins" / "FindTheUnencryptedDotText" / "SporeAppRelease.exe",
    _ROOT / "refs" / "bins" / "FindTheUnencryptedDotText" / "SporeCreatureCreatorCSA2024.exe",
    _ROOT / "refs" / "bins" / "Proto" / "GonzagoGL.exe",
]
OUT = _ROOT / "analysis" / "build_identity.md"


def info(path: Path) -> dict:
    d: dict = {"path": path}
    try:
        pe = pefile.PE(str(path), fast_load=True)
    except Exception as exc:  # noqa: BLE001
        d["error"] = f"{type(exc).__name__}: {exc}"
        return d
    oh = pe.OPTIONAL_HEADER
    fh = pe.FILE_HEADER
    d["machine"] = fh.Machine
    d["timestamp"] = fh.TimeDateStamp
    d["entry"] = pe.OPTIONAL_HEADER.ImageBase + oh.AddressOfEntryPoint
    d["image_base"] = oh.ImageBase
    d["size_of_image"] = oh.SizeOfImage
    d["linker"] = (oh.MajorLinkerVersion, oh.MinorLinkerVersion)
    d["sections"] = {}
    for s in pe.sections:
        name = s.Name.rstrip(b"\x00").decode("latin-1")
        blob = pe.__data__[s.PointerToRawData : s.PointerToRawData + s.SizeOfRawData]
        d["sections"][name] = {
            "vsize": s.Misc_VirtualSize,
            "rsize": s.SizeOfRawData,
            "va": oh.ImageBase + s.VirtualAddress,
            "sha256": hashlib.sha256(blob).hexdigest() if blob else "",
            "len": len(blob),
        }
    # CodeView
    full = pefile.PE(str(path))
    d["codeview"] = None
    if hasattr(full, "DIRECTORY_ENTRY_DEBUG"):
        for e in full.DIRECTORY_ENTRY_DEBUG:
            st = e.struct
            if st.PointerToRawData and st.SizeOfData:
                raw = full.__data__[st.PointerToRawData : st.PointerToRawData + st.SizeOfData]
                if raw[:4] == b"RSDS":
                    import uuid

                    d["codeview"] = {
                        "guid": str(uuid.UUID(bytes_le=raw[4:20])),
                        "age": struct.unpack("<I", raw[20:24])[0],
                        "pdb": raw[24:].split(b"\x00")[0].decode("latin-1"),
                    }
    # imports (dll names + count of thunks, as a fingerprint)
    full.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
    imps = []
    for e in getattr(full, "DIRECTORY_ENTRY_IMPORT", []):
        names = sorted((i.name.decode("latin-1") if i.name else f"#{i.ordinal}") for i in e.imports)
        imps.append((e.dll.decode("latin-1").lower(), names))
    d["imports"] = imps
    return d


def rel(p: Path) -> str:
    try:
        return str(p.relative_to(_ROOT))
    except ValueError:
        return str(p)


def main() -> int:
    t = info(TARGET)
    rows = [(c, info(c)) for c in CANDIDATES]
    L: list[str] = []
    L.append("# Which binary is the *same build* as the reimplementation target?")
    L.append("")
    L.append(f"Target: `{rel(TARGET)}` (encrypted .text).")
    L.append("")
    L.append("## Target fingerprint")
    L.append("")
    L.append(f"- timestamp      : {t['timestamp']} ({_ts(t['timestamp'])})")
    L.append(f"- entry point    : 0x{t['entry']:08x}")
    L.append(f"- size_of_image  : 0x{t['size_of_image']:x}")
    L.append(f"- linker         : {t['linker'][0]}.{t['linker'][1]}")
    L.append(f"- codeview       : {t.get('codeview')}")
    for name, s in sorted(t["sections"].items()):
        L.append(f"- section `{name}`: va=0x{s['va']:08x} vsize=0x{s['vsize']:x} raw=0x{s['rsize']:x} sha256={s['sha256'][:16]}…")
    L.append("")
    L.append("## Candidate comparison")
    L.append("")
    L.append("| candidate | ts equal | codeview equal | .rdata equal | .data equal | .text equal | import-dlls equal | verdict |")
    L.append("|---|---|---|---|---|---|---|---|")
    for c, i in rows:
        if "error" in i:
            L.append(f"| `{rel(c)}` | - | - | - | - | - | - | ERROR {i['error']} |")
            continue
        def sec(name):
            return i["sections"].get(name, {}).get("sha256", "")
        def tsec(name):
            return t["sections"].get(name, {}).get("sha256", "")
        cv_t, cv_i = t.get("codeview"), i.get("codeview")
        cv_eq = cv_t is not None and cv_i is not None and (cv_t["guid"], cv_t["age"]) == (cv_i["guid"], cv_i["age"])
        rd_eq = bool(sec(".rdata")) and sec(".rdata") == tsec(".rdata")
        da_eq = bool(sec(".data")) and sec(".data") == tsec(".data")
        tx_eq = bool(sec(".text")) and sec(".text") == tsec(".text")
        dlls_t = {n for n, _ in t["imports"]}
        dlls_i = {n for n, _ in i["imports"]}
        im_eq = dlls_t == dlls_i
        verdict = []
        if cv_eq:
            verdict.append("**SAME LINK (codeview match)**")
        if rd_eq and da_eq:
            verdict.append("data-identical")
        if tx_eq:
            verdict.append("text-identical")
        if not verdict:
            verdict.append("different build")
        L.append(
            f"| `{rel(c)}` | {'y' if i['timestamp']==t['timestamp'] else 'n'} | "
            f"{'**y**' if cv_eq else 'n'} | {'**y**' if rd_eq else 'n'} | {'**y**' if da_eq else 'n'} | "
            f"{'y' if tx_eq else 'n'} | {'y' if im_eq else 'n'} | {', '.join(verdict)} |"
        )
    L.append("")
    L.append("## Details per candidate")
    L.append("")
    for c, i in rows:
        if "error" in i:
            L.append(f"### `{rel(c)}`\n\nERROR: {i['error']}\n")
            continue
        L.append(f"### `{rel(c)}`")
        L.append("")
        L.append(f"- timestamp: {i['timestamp']} ({_ts(i['timestamp'])})   entry: 0x{i['entry']:08x}   "
                 f"image_base: 0x{i['image_base']:08x}   size_of_image: 0x{i['size_of_image']:x}   "
                 f"linker: {i['linker'][0]}.{i['linker'][1]}")
        L.append(f"- codeview: {i.get('codeview')}")
        L.append(f"- import DLLs ({len(i['imports'])}): {', '.join(sorted(n for n,_ in i['imports']))}")
        L.append("")
        L.append("| section | va | vsize | raw | sha256[:16] | vs target |")
        L.append("|---|---|---|---|---|---|")
        for name, s in sorted(i["sections"].items()):
            ts = t["sections"].get(name)
            if ts is None:
                cmp_ = "no such section in target"
            elif s["sha256"] == ts["sha256"]:
                cmp_ = "**identical**"
            else:
                cmp_ = f"differs (target sha {ts['sha256'][:12]}…, vsize 0x{ts['vsize']:x})"
            L.append(f"| `{name}` | 0x{s['va']:08x} | 0x{s['vsize']:x} | 0x{s['rsize']:x} | {s['sha256'][:16]} | {cmp_} |")
        L.append("")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(L) + "\n", encoding="utf-8")
    print("\n".join(L[:40]))
    print(f"\n… wrote {OUT.relative_to(_ROOT)}")
    return 0


def _ts(v: int) -> str:
    import datetime

    try:
        return datetime.datetime.fromtimestamp(v, datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC")
    except (OverflowError, OSError, ValueError):
        return f"<invalid {v:#x}>"


if __name__ == "__main__":
    raise SystemExit(main())
